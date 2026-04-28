import argparse
import json
import os
import re
from pathlib import Path
import sys
import cv2
import easyocr
import numpy as np
from ultralytics import YOLO
import warnings
warnings.filterwarnings("ignore")

BASE_DIR = Path(__file__).resolve().parent
DEFAULT_MODEL_PATH = BASE_DIR.parent / "AI model" / "models" / "license_plate_detector.pt"

# Load 1 lần (tăng tốc)
detector = YOLO(str(DEFAULT_MODEL_PATH))
reader = easyocr.Reader(["en"], gpu=False)
def fix_ocr_vn(text: str) -> str:
    chars = list(text)

    if len(chars) < 3:
        return text

    # 2 ký tự đầu phải là số
    for i in range(min(2, len(chars))):
        if chars[i] == 'O':
            chars[i] = '0'

    # ký tự thứ 3 phải là chữ
    if len(chars) > 2 and chars[2] == '0':
        chars[2] = 'O'

    # phần sau phải là số
    for i in range(3, len(chars)):
        if chars[i] == 'O':
            chars[i] = '0'

    return "".join(chars)

def normalize_text(text: str) -> str:
    return re.sub(r"[^A-Za-z0-9]", "", text or "").upper()


def preprocess_variants(plate_crop: np.ndarray) -> list[np.ndarray]:
    """Trả về nhiều phiên bản preprocess để tăng tỉ lệ OCR thành công."""
    variants = []

    # Upscale trước
    upscaled = cv2.resize(plate_crop, None, fx=2, fy=2, interpolation=cv2.INTER_CUBIC)
    gray = cv2.cvtColor(upscaled, cv2.COLOR_BGR2GRAY)

    # 1. CLAHE (cân bằng sáng tốt hơn threshold cứng)
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    clahe_img = clahe.apply(gray)
    variants.append(clahe_img)

    # 2. Otsu threshold (tự động tìm ngưỡng)
    _, otsu = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    variants.append(otsu)

    # 3. Adaptive threshold (tốt cho ánh sáng không đều)
    adaptive = cv2.adaptiveThreshold(
        gray, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY, 11, 2
    )
    variants.append(adaptive)

    # 4. Ảnh gốc upscale (không threshold, đôi khi OCR tốt hơn)
    variants.append(upscaled)

    return variants


def ocr_best_result(variants: list[np.ndarray], debug: bool = False) -> list[tuple]:
    """Chạy OCR trên nhiều variants, trả về kết quả tốt nhất."""
    best_results = []
    best_score = -1

    for i, img in enumerate(variants):
        try:
            results = reader.readtext(img, detail=1, paragraph=False)
            if not results:
                continue

            score = sum(prob for _, _, prob in results if prob > 0.2)

            if debug:
                texts = [(t, round(p, 2)) for _, t, p in results]
                print(f"[DEBUG] Variant {i}: score={score:.2f}, texts={texts}", flush=True)

            if score > best_score:
                best_score = score
                best_results = results
        except Exception as e:
            if debug:
                print(f"[DEBUG] Variant {i} OCR error: {e}", flush=True)

    return best_results


# Regex biển số VN đầy đủ hơn
VN_PLATE_PATTERNS = [
    r"\d{2}[A-Z]\d{4,5}",        # Ví dụ: 51F12345
    r"\d{2}[A-Z]{1,2}\d{4,5}",   # Ví dụ: 51AB1234
    r"\d{2}[A-Z]\d{3,4}[A-Z]",   # Một số biển đặc biệt
]


def extract_vn_plate(text: str) -> str:
    """Thử nhiều pattern để trích xuất biển số VN."""
    for pattern in VN_PLATE_PATTERNS:
        match = re.search(pattern, text)
        if match:
            return match.group()
    return ""


def detect_license_plate(
    image_path: str,
    uid: str,
    token: str,
    mock: bool = False,
    debug: bool = False,
) -> dict:
    frame = cv2.imread(image_path)

    if frame is None:
        return {
            "success": False,
            "error": f"Unable to read image: {image_path}",
            "uid": uid,
            "token": token,
            "image_path": image_path,
            "plate_text": "",
            "detected": False,
        }

    if mock:
        return {
            "success": True,
            "uid": uid,
            "token": token,
            "image_path": image_path,
            "plate_text": "MOCK1234",
            "detected": True,
            "mock": True,
        }

    results = detector(frame)
    final_plate = ""
    all_candidates = []  # Lưu tất cả candidates để debug

    for result in results:
        # Sắp xếp box theo confidence (cao nhất trước)
        boxes = sorted(result.boxes, key=lambda b: float(b.conf[0]), reverse=True)

        for box in boxes:
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            conf = float(box.conf[0])
            height, width = frame.shape[:2]

            # Clamp tọa độ
            x1, y1 = max(0, x1), max(0, y1)
            x2, y2 = min(width, x2), min(height, y2)

            if x2 <= x1 or y2 <= y1:
                continue

            plate_crop = frame[y1:y2, x1:x2]
            if plate_crop.size == 0:
                continue

            if debug:
                print(f"[DEBUG] Box conf={conf:.2f}, crop shape={plate_crop.shape}", flush=True)

            # Thử nhiều preprocessing
            variants = preprocess_variants(plate_crop)
            ocr_results = ocr_best_result(variants, debug=debug)

            # Sort theo tọa độ (biển 2 dòng VN)
            ocr_results = sorted(ocr_results, key=lambda x: (x[0][0][1], x[0][0][0]))

            # Thu thập tất cả text có prob > 0.2 (thấp hơn để không bỏ sót)
            texts = [text for _, text, prob in ocr_results if prob > 0.2]
            raw_text = normalize_text("".join(texts))
            raw_text = fix_ocr_vn(raw_text)  

            if debug:
                print(f"[DEBUG] Raw OCR text: {''.join(texts)!r} → normalized: {raw_text!r}", flush=True)

            all_candidates.append(raw_text)

            plate_text = extract_vn_plate(raw_text)
            if plate_text:
                final_plate = plate_text
                break

        if final_plate:
            break

    # Fallback: nếu không match pattern, trả về text dài nhất từ candidates
    if not final_plate and all_candidates:
        longest = max(all_candidates, key=len)
        if debug:
            print(f"[DEBUG] No pattern match. Best candidate fallback: {longest!r}", flush=True)
        # Vẫn trả về để caller quyết định, không ép pattern
        final_plate = longest  # Bỏ dòng này nếu muốn strict VN format

    return {
        "success": True,
        "uid": uid,
        "token": token,
        "image_path": image_path,
        "plate_text": final_plate,
        "detected": bool(final_plate),
    }


import sys

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image_path")
    parser.add_argument("uid")
    parser.add_argument("token")
    parser.add_argument("--mock", action="store_true")
    parser.add_argument("--debug", action="store_true")

    args = parser.parse_args()

    result = detect_license_plate(
        args.image_path,
        args.uid,
        args.token,
        args.mock,
        args.debug,
    )

    print(json.dumps(result), flush=True)

if __name__ == "__main__":
    main()