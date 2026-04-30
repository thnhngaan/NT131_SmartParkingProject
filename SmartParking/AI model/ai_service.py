from flask import Flask, jsonify, request
from prophet import Prophet
import pandas as pd
from pymongo import MongoClient
from datetime import datetime, timedelta, timezone
import logging
import traceback
from flask_cors import CORS
import numpy as np

# ===== CONFIG =====
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

app = Flask(__name__)
CORS(app)

PREDICTION_PERIODS = 24
MIN_ROWS_FOR_PROPHET = 6   # ⚠️ tăng lên để tránh đoán bậy
MAX_CAPACITY = 300         # ⚠️ sức chứa bãi xe

VN_TZ = timezone(timedelta(hours=7))

# ===== DATABASE =====
def get_collection():
    try:
        client = MongoClient("mongodb://localhost:27017/", serverSelectionTimeoutMS=3000)
        client.server_info()
        return client["smartparking"]["parkings"]
    except Exception as e:
        logger.error(f"MongoDB connection failed: {e}")
        return None


# ===== BUILD FLOW =====
def _build_flow_hourly_df(raw_records):
    if not raw_records:
        return pd.DataFrame(columns=["ds", "in", "out"])

    df = pd.DataFrame(raw_records)

    if df.empty:
        return pd.DataFrame(columns=["ds", "in", "out"])

    df["entryTime"] = pd.to_datetime(df.get("entryTime"), errors="coerce", utc=True)
    df["exitTime"] = pd.to_datetime(df.get("exitTime"), errors="coerce", utc=True)

    # IN
    in_df = df.dropna(subset=["entryTime"]).copy()
    in_df["ds"] = in_df["entryTime"].dt.tz_convert(None).dt.floor("h")
    in_df = in_df["ds"].value_counts().rename_axis("ds").reset_index(name="in")

    # OUT
    out_df = df.dropna(subset=["exitTime"]).copy()
    out_df["ds"] = out_df["exitTime"].dt.tz_convert(None).dt.floor("h")
    out_df = out_df["ds"].value_counts().rename_axis("ds").reset_index(name="out")

    flow_df = pd.merge(in_df, out_df, how="outer", on="ds").fillna(0)

    flow_df["in"] = flow_df["in"].astype(int)
    flow_df["out"] = flow_df["out"].astype(int)

    # 🚨 lọc dữ liệu bất thường (anti-spam test)
    flow_df = flow_df[
        (flow_df["in"] <= 100) &
        (flow_df["out"] <= 100)
    ]

    return flow_df.sort_values("ds")


# ===== GET DATA =====
def get_data(days=7):
    try:
        col = get_collection()
        if col is None:
            return pd.DataFrame(columns=["ds", "in", "out"])

        cutoff = datetime.now(VN_TZ) - timedelta(days=days)

        cursor = col.find(
            {
                "createdAt": {"$gte": cutoff},
                "$or": [
                    {"entryTime": {"$ne": None}},
                    {"exitTime": {"$ne": None}}
                ]
            },
            {
                "entryTime": 1,
                "exitTime": 1,
                "_id": 0
            }
        )

        raw_entries = list(cursor)
        logger.debug(f"Fetched {len(raw_entries)} records")

        return _build_flow_hourly_df(raw_entries)

    except Exception:
        logger.error(f"get_data() failed:\n{traceback.format_exc()}")
        return pd.DataFrame(columns=["ds", "in", "out"])


# ===== PROPHET =====
def _predict_single_flow(flow_df, target_col):
    if flow_df is None or flow_df.empty:
        return None

    train = flow_df[["ds", target_col]].rename(columns={target_col: "y"}).copy()
    train["ds"] = pd.to_datetime(train["ds"])
    train["y"] = pd.to_numeric(train["y"], errors="coerce")

    train = train.dropna()

    if len(train) < MIN_ROWS_FOR_PROPHET:
        return None

    # 🔥 log scale để giảm spike
    train["y"] = np.log1p(train["y"])

    model = Prophet(daily_seasonality=True, weekly_seasonality=False)
    model.fit(train)

    future = model.make_future_dataframe(periods=PREDICTION_PERIODS, freq="h")
    forecast = model.predict(future)[["ds", "yhat"]].tail(PREDICTION_PERIODS)

    # 🔥 inverse log
    forecast["yhat"] = np.expm1(forecast["yhat"])

    # 🔥 clamp theo sức chứa
    forecast["yhat"] = forecast["yhat"].clip(lower=0, upper=MAX_CAPACITY)

    forecast["yhat"] = forecast["yhat"].round().astype(int)

    return forecast


# ===== BASELINE =====
def _safe_baseline_predictions(flow_df):
    now = pd.Timestamp(datetime.now(VN_TZ)).floor("h")

    mean_in = 0
    mean_out = 0

    if flow_df is not None and not flow_df.empty:
        mean_in = int(flow_df["in"].mean())
        mean_out = int(flow_df["out"].mean())

    future_dates = pd.date_range(start=now, periods=PREDICTION_PERIODS + 1, freq="h")[1:]

    result = pd.DataFrame({
        "ds": future_dates,
        "in": [min(mean_in, MAX_CAPACITY)] * PREDICTION_PERIODS,
        "out": [min(mean_out, MAX_CAPACITY)] * PREDICTION_PERIODS
    })

    result["ds"] = result["ds"].dt.strftime("%Y-%m-%d %H:%M:%S")
    return result.to_dict(orient="records")


# ===== ROUTES =====
@app.route('/')
def home():
    return "OK SERVER"


@app.route('/predict')
def predict():
    try:
        df = get_data()

        if df.empty or len(df) < MIN_ROWS_FOR_PROPHET:
            return jsonify(_safe_baseline_predictions(df))

        in_forecast = _predict_single_flow(df, "in")
        out_forecast = _predict_single_flow(df, "out")

        if in_forecast is None or out_forecast is None:
            return jsonify(_safe_baseline_predictions(df))

        merged = pd.DataFrame({
            "ds": in_forecast["ds"],
            "in": in_forecast["yhat"],
            "out": out_forecast["yhat"]
        })

        merged["ds"] = merged["ds"].dt.strftime("%Y-%m-%d %H:%M:%S")

        return jsonify(merged.to_dict(orient="records"))

    except Exception:
        logger.error(f"/predict failed:\n{traceback.format_exc()}")
        return jsonify(_safe_baseline_predictions(None))


# ===== HISTORY =====
@app.route('/history')
def history():
    try:
        col = get_collection()
        if col is None:
            return jsonify([])

        date_str = request.args.get("date")
        if not date_str:
            date_str = datetime.now(VN_TZ).strftime("%Y-%m-%d")

        start = datetime.strptime(date_str, "%Y-%m-%d").replace(tzinfo=VN_TZ)
        end = start + timedelta(days=1)

        cursor = col.find(
            {
                "createdAt": {"$gte": start, "$lt": end}
            },
            {
                "uid": 1,
                "slotNumber": 1,
                "entryTime": 1,
                "exitTime": 1,
                "_id": 0
            }
        )

        data = list(cursor)

        for d in data:
            if d.get("entryTime"):
                d["entryTime"] = str(d["entryTime"])
            if d.get("exitTime"):
                d["exitTime"] = str(d["exitTime"])

        return jsonify(data)

    except Exception:
        logger.error(f"/history failed:\n{traceback.format_exc()}")
        return jsonify([])


# ===== RUN =====
if __name__ == "__main__":
    app.run(debug=True, port=5000)