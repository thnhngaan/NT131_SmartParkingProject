from ultralytics import YOLO
import cv2
import os
import easyocr

# Load model detect
license_plate_detector = YOLO("./models/license_plate_detector.pt")

# Load OCR
reader = easyocr.Reader(['en'])  # biển số VN vẫn đọc tốt với en

# Thư mục ảnh
input_folder = "./input"
output_folder = "./out"
os.makedirs(output_folder, exist_ok=True)

image_files = [f for f in os.listdir(input_folder) if f.lower().endswith((".jpg", ".png", ".jpeg"))]

for img_name in image_files:
    img_path = os.path.join(input_folder, img_name)
    frame = cv2.imread(img_path)

    if frame is None:
        print(f"Lỗi đọc ảnh: {img_name}")
        continue

    results = license_plate_detector(frame)

    for result in results:
        for bbox in result.boxes:
            x1, y1, x2, y2 = map(int, bbox.xyxy[0])

            # Crop biển số
            plate_crop = frame[y1:y2, x1:x2]

            # OCR
            ocr_result = reader.readtext(plate_crop)

            plate_text = ""
            for (bbox, text, prob) in ocr_result:
                if prob > 0.3:
                    plate_text += text + " "

            plate_text = plate_text.strip()

            # Vẽ khung
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)

            # Hiển thị text lên ảnh
            cv2.putText(frame, plate_text, (x1, y1 - 10),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.8, (0, 255, 0), 2)

            print(f"{img_name} -> {plate_text}")

    # Lưu ảnh
    output_path = os.path.join(output_folder, img_name)
    cv2.imwrite(output_path, frame)

print("Xong!")