# Kế hoạch triển khai tách GUI và Core Backend Service

## Mục tiêu

Chuyển ứng dụng hiện tại từ mô hình single-process desktop sang mô hình GUI client + core backend service, đồng thời giữ được trải nghiệm người dùng tốt hơn.

## Phạm vi

- Tách logic xử lý capture khỏi UI.
- Tạo boundary giữa GUI và backend.
- Cung cấp command và event protocol.
- Cho phép GUI nhận realtime update từ backend.

## Phase 0 - Chuẩn bị nền tảng

- [ ] Xác định lại các module nào thuộc UI và module nào thuộc core.
- [ ] Tạo tài liệu kiến trúc và API contract.
- [ ] Định nghĩa message schema cho command/event.
- [ ] Chọn transport: HTTP/JSON cho command, WebSocket cho event.

## Phase 1 - Tạo service boundary

- [ ] Tạo interface/contract chung cho backend operations.
- [ ] Tách logic capture pipeline khỏi lớp UI controller hiện tại.
- [ ] Đảm bảo hiện tại vẫn build và test bình thường.

## Phase 2 - Backend service đầu tiên

- [ ] Tạo executable backend service mới chạy nền.
- [ ] Backend hỗ trợ lệnh: start, stop, status.
- [ ] Backend ghi kết quả vào DB và tạo session record.
- [ ] GUI có thể gọi backend bằng command API.

## Phase 3 - Realtime update

- [ ] Thêm WebSocket server ở backend.
- [ ] GUI subscribe event và cập nhật dữ liệu realtime.
- [ ] Loại bỏ hoặc giảm polling hiện tại trong UI.

## Phase 4 - Hardening

- [ ] Thêm logging, error handling, retry.
- [ ] Thêm health check và metrics.
- [ ] Hỗ trợ cấu hình SQLite local.
- [ ] Viết test cho protocol và integration flow.

## Deliverables

1. Tài liệu kiến trúc service.
2. Backend service skeleton.
3. Command/event protocol.
4. GUI kết nối backend và nhận update realtime.
5. Hướng dẫn vận hành và demo.

## Ưu tiên triển khai

1. Tách logic xử lý khỏi UI controller.
2. Tạo backend chạy độc lập.
3. Kết nối GUI bằng command API.
4. Thêm realtime stream qua WebSocket.
5. Tối ưu production readiness.
