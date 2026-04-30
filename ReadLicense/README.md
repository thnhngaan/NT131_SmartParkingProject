# Nhận Dạng Biển Số Xe Tự Động Sử Dụng YOLOv11

- [Nhận Dạng Biển Số Xe Tự Động Sử Dụng YOLOv11](#nhận-dạng-biển-số-xe-tự-động-sử-dụng-yolov11)
  - [Dữ Liệu](#dữ-liệu)
  - [Mô Hình](#mô-hình)
  - [Các Phụ Thuộc](#các-phụ-thuộc)
  - [Thiết Lập Dự Án](#thiết-lập-dự-án)

## Dữ Liệu

Bộ dữ liệu **Nhận Dạng Biển Số Xe** được sử dụng để huấn luyện mô hình này có thể tìm thấy [ở đây](https://universe.roboflow.com/roboflow-universe-projects/license-plate-recognition-rxg4e/dataset/4).

Video được sử dụng trong dự án này có thể tìm thấy [ở đây](https://www.pexels.com/video/cars-are-driving-on-a-snowy-road-in-the-city-9487043/).

## Mô Hình

Một mô hình dò biển số xe được sử dụng để phát hiện các biển số. Mô hình được huấn luyện bằng YOLOv11 cho **100** epoch với **21173** hình ảnh có kích thước `640x640`.

Mô hình đã được huấn luyện có sẵn [ở đây](./models/license_plate_detector.pt).

## Các Phụ Thuộc

- Python 3.x
- opencv_contrib_python
- opencv_python
- ultralytics

## Thiết Lập Dự Án

- Tạo môi trường ảo bằng lệnh sau:

  ```bash
  python3 -m venv myenv
  ```

  Thay thế `myenv` bằng tên bạn muốn cho môi trường ảo của mình. Điều này sẽ tạo một thư mục có tên myenv trong thư mục hiện tại của bạn chứa các tệp môi trường ảo.

- Kích hoạt môi trường ảo:

  ```bash
  source myenv/bin/activate
  ```

  Hãy nhớ thay thế `myenv` bằng tên thực tế của môi trường được tạo trong bước trước.

- Điều hướng đến thư mục gốc của dự án:

  ```bash
  cd path/to/the/project
  ```

- Cài đặt các phụ thuộc:
  ```bash
  pip install -r requirements.txt
  ```

- Để thực thi tập lệnh, chạy:

  ```bash
  python3 main.py
  ```

- Khi bạn hoàn thành việc làm việc trong môi trường ảo, bạn có thể vô hiệu hóa nó bằng cách chạy:
  ```bash
  deactivate
  ```