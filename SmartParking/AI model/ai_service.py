from flask import Flask, jsonify, request
from prophet import Prophet
import pandas as pd
from pymongo import MongoClient
from datetime import datetime, timedelta
import logging
import traceback  # ← THÊM
logging.basicConfig(level=logging.DEBUG)  # ← THÊM — log ra terminal
logger = logging.getLogger(__name__)
from flask_cors import CORS 
app = Flask(__name__)
CORS(app)

PREDICTION_PERIODS = 24
MIN_ROWS_FOR_PROPHET = 2
DEFAULT_BASELINE = 0.0

# ← Lazy connection — không connect ở module level
def get_collection():
    try:
        client = MongoClient("mongodb://localhost:27017/", serverSelectionTimeoutMS=3000)
        client.server_info()  # trigger connection check
        return client["smartparking"]["parkingslots"]
    except Exception as e:
        logger.error(f"MongoDB connection failed: {e}")
        return None


def _build_flow_hourly_df(raw_records):
    if not raw_records:
        return pd.DataFrame(columns=["ds", "in", "out"])

    df = pd.DataFrame(raw_records)
    if "entryTime" not in df.columns and "exitTime" not in df.columns:
        return pd.DataFrame(columns=["ds", "in", "out"])

    entry_series = pd.Series([], dtype="datetime64[ns]")
    exit_series = pd.Series([], dtype="datetime64[ns]")

    if "entryTime" in df.columns:
        entry_series = pd.to_datetime(df["entryTime"], errors="coerce", utc=True).dropna()
        if not entry_series.empty:
            entry_series = entry_series.dt.tz_convert(None).dt.floor("h")

    if "exitTime" in df.columns:
        exit_series = pd.to_datetime(df["exitTime"], errors="coerce", utc=True).dropna()
        if not exit_series.empty:
            exit_series = exit_series.dt.tz_convert(None).dt.floor("h")

    in_df = pd.DataFrame(columns=["ds", "in"])
    out_df = pd.DataFrame(columns=["ds", "out"])

    if not entry_series.empty:
        in_df = entry_series.value_counts().rename_axis("ds").reset_index(name="in")
    if not exit_series.empty:
        out_df = exit_series.value_counts().rename_axis("ds").reset_index(name="out")

    flow_df = pd.merge(in_df, out_df, how="outer", on="ds").fillna(0)
    if flow_df.empty:
        return pd.DataFrame(columns=["ds", "in", "out"])

    flow_df["in"] = pd.to_numeric(flow_df["in"], errors="coerce").fillna(0).clip(lower=0).round().astype(int)
    flow_df["out"] = pd.to_numeric(flow_df["out"], errors="coerce").fillna(0).clip(lower=0).round().astype(int)
    flow_df = flow_df.dropna(subset=["ds"]).sort_values("ds")
    return flow_df[["ds", "in", "out"]]


def _predict_single_flow(flow_df, target_col, periods=PREDICTION_PERIODS):
    if flow_df is None or flow_df.empty or target_col not in flow_df.columns:
        return None

    train = flow_df[["ds", target_col]].rename(columns={target_col: "y"}).copy()
    train["ds"] = pd.to_datetime(train["ds"], errors="coerce")
    train["y"] = pd.to_numeric(train["y"], errors="coerce")
    train = train.dropna(subset=["ds", "y"]).sort_values("ds")

    if train.empty or len(train) < MIN_ROWS_FOR_PROPHET:
        return None

    model = Prophet(daily_seasonality=True, weekly_seasonality=False, changepoint_prior_scale=0.1)
    model.fit(train)
    future = model.make_future_dataframe(periods=periods, freq="h")
    forecast = model.predict(future)[["ds", "yhat"]].tail(periods).copy()
    forecast["yhat"] = forecast["yhat"].clip(lower=0).round().astype(int)
    return forecast


def _safe_baseline_predictions(flow_df, periods=PREDICTION_PERIODS):
    if flow_df is not None and not flow_df.empty:
        last_ds = pd.to_datetime(flow_df["ds"], errors="coerce").max()
        mean_in = float(pd.to_numeric(flow_df["in"], errors="coerce").dropna().mean()) if "in" in flow_df.columns else 0
        mean_out = float(pd.to_numeric(flow_df["out"], errors="coerce").dropna().mean()) if "out" in flow_df.columns else 0
        if pd.isna(last_ds):
            last_ds = pd.Timestamp(datetime.utcnow()).floor("h")
    else:
        last_ds = pd.Timestamp(datetime.utcnow()).floor("h")
        mean_in = DEFAULT_BASELINE
        mean_out = DEFAULT_BASELINE

    mean_in = int(max(round(mean_in), 0))
    mean_out = int(max(round(mean_out), 0))
    future_dates = pd.date_range(start=last_ds, periods=periods + 1, freq="h")[1:]
    result = pd.DataFrame({"ds": future_dates, "in": [mean_in] * periods, "out": [mean_out] * periods})
    result["ds"] = result["ds"].dt.strftime("%Y-%m-%d %H:%M:%S")
    return result.to_dict(orient="records")


def get_data(days=7):
    try:
        col = get_collection()
        if col is None:
            return pd.DataFrame(columns=["ds", "in", "out"])

        cutoff = datetime.utcnow() - timedelta(days=days)

        cursor = col.find(
            {
                "$or": [
                    {"entryTime": {"$exists": True, "$ne": None, "$gte": cutoff}},
                    {"exitTime": {"$exists": True, "$ne": None, "$gte": cutoff}}
                ]
            },
            {"entryTime": 1, "exitTime": 1, "_id": 0}
        )
        raw_entries = list(cursor)
        logger.debug(f"Fetched {len(raw_entries)} records from MongoDB")

        return _build_flow_hourly_df(raw_entries)

    except Exception:
        logger.error(f"get_data() failed:\n{traceback.format_exc()}")
        return pd.DataFrame(columns=["ds", "in", "out"])


@app.route('/')
def home():
    return "OK SERVER"


@app.route('/predict')
def predict():
    try:
        df = get_data()
        logger.debug(f"DataFrame shape: {df.shape}")  # ← log để biết có data không

        if df is None or df.empty or len(df) < MIN_ROWS_FOR_PROPHET:
            logger.warning(f"Not enough data for Prophet (rows={len(df) if df is not None else 0}), using baseline")
            return jsonify(_safe_baseline_predictions(df, periods=PREDICTION_PERIODS))

        df = df.copy()
        df["ds"] = pd.to_datetime(df["ds"], errors="coerce")
        df["in"] = pd.to_numeric(df["in"], errors="coerce")
        df["out"] = pd.to_numeric(df["out"], errors="coerce")
        df = df.dropna(subset=["ds"]).sort_values("ds")

        in_forecast = _predict_single_flow(df, "in", periods=PREDICTION_PERIODS)
        out_forecast = _predict_single_flow(df, "out", periods=PREDICTION_PERIODS)

        if in_forecast is None and out_forecast is None:
            return jsonify(_safe_baseline_predictions(df, periods=PREDICTION_PERIODS))

        if in_forecast is None or out_forecast is None:
            baseline = pd.DataFrame(_safe_baseline_predictions(df, periods=PREDICTION_PERIODS))
            baseline["ds"] = pd.to_datetime(baseline["ds"], errors="coerce")
            if in_forecast is None:
                in_forecast = baseline[["ds", "in"]].rename(columns={"in": "yhat"})
            if out_forecast is None:
                out_forecast = baseline[["ds", "out"]].rename(columns={"out": "yhat"})

        merged = pd.DataFrame({
            "ds": in_forecast["ds"],
            "in": in_forecast["yhat"].clip(lower=0).round().astype(int),
            "out": out_forecast["yhat"].clip(lower=0).round().astype(int),
        }).tail(PREDICTION_PERIODS)

        merged["ds"] = merged["ds"].dt.strftime("%Y-%m-%d %H:%M:%S")
        return jsonify(merged.to_dict(orient="records"))

    except Exception:
        logger.error(f"/predict failed:\n{traceback.format_exc()}")  # ← log full lỗi Prophet
        return jsonify(_safe_baseline_predictions(None, periods=PREDICTION_PERIODS))


@app.route('/history')
def history():
    try:
        date_str = request.args.get("date")
        if not date_str:
            date_str = datetime.utcnow().strftime("%Y-%m-%d")

        target_day = pd.to_datetime(date_str, errors="coerce")
        if pd.isna(target_day):
            return jsonify({"error": "Invalid date format. Use YYYY-MM-DD"}), 400

        day_str = target_day.strftime("%Y-%m-%d")
        df = get_data(days=30)
        if df is None or df.empty:
            rows = [{"ds": f"{day_str} {str(h).zfill(2)}:00:00", "in": 0, "out": 0} for h in range(24)]
            return jsonify(rows)

        df = df.copy()
        df["ds"] = pd.to_datetime(df["ds"], errors="coerce")
        df = df.dropna(subset=["ds"])
        df["date_only"] = df["ds"].dt.strftime("%Y-%m-%d")
        day_df = df[df["date_only"] == day_str][["ds", "in", "out"]]

        full_hours = pd.DataFrame({
            "ds": pd.date_range(start=f"{day_str} 00:00:00", periods=24, freq="h")
        })
        merged = full_hours.merge(day_df, on="ds", how="left").fillna(0)
        merged["in"] = pd.to_numeric(merged["in"], errors="coerce").fillna(0).clip(lower=0).round().astype(int)
        merged["out"] = pd.to_numeric(merged["out"], errors="coerce").fillna(0).clip(lower=0).round().astype(int)
        merged["ds"] = merged["ds"].dt.strftime("%Y-%m-%d %H:%M:%S")
        return jsonify(merged.to_dict(orient="records"))
    except Exception:
        logger.error(f"/history failed:\n{traceback.format_exc()}")
        return jsonify([])


if __name__ == "__main__":
    app.run(debug=True, port=5000)