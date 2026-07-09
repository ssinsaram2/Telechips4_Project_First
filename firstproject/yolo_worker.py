import cv2
import socket
from ultralytics import YOLO

class YoloWorker:
    def __init__(self, model_path="yolov8n_ncnn_model", rpi_ip="192.168.111.112", rpi_port=5000):
        # 1. YOLO 모델 로드
        self.model = YOLO(model_path)
        print("[YOLO] NCNN 경량화 모델 로드 완료")
        
        # 2. [추가] 라즈베리파이4 송신용 UDP 소켓 설정
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.rpi_address = (rpi_ip, rpi_port)
        
        try:
            self.client_socket.connect(self.rpi_address)
            print(f"[YOLO] 라파4({rpi_ip}:{rpi_port}) 데이터 연결 성공")
        except Exception as e:
            print(f"[YOLO 네트워크 에러] 연결 실패 (로컬로만 실행): {e}")
            self.client_socket = None
            
        # 3. [추가] 상태 추적용 변수 (None, 0: 없음, 1: 있음)
        self.last_state = None 

    def process_frame(self, frame):
        """메인에서 프레임을 받아 객체 추론을 수행하고 이더넷 신호를 송신합니다."""
        results = self.model(frame, classes=0, verbose=False)
        person_count = 0
        
        if len(results) > 0:
            r = results[0]
            person_count = len(r.boxes)
            
            # 화면 드로잉 (바운딩 박스)
            for box in r.boxes:
                x1, y1, x2, y2 = map(int, box.xyxy[0].tolist())
                conf = float(box.conf[0])
                cv2.rectangle(frame, (x1, y1), (x2, y2), (255, 0, 0), 2)
                cv2.putText(frame, f"Person: {conf:.2f}", (x1, y1 - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2)
                
        # 좌측 상단에 인원 수 표시
        cv2.putText(frame, f"Count: {person_count}", (10, 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 255), 2)
        
        # ============================================================
        # [신규 추가] 사람이 감지 여부에 따른 이더넷 (UDP) 데이터 송신
        # ============================================================
        current_state = 1 if person_count > 0 else 0
        
        # 상태가 변했을 때만 패킷을 전송하여 네트워크 부하 최적화 (Edge Trigger)
        if current_state != self.last_state:
            if self.client_socket is not None:
                try:
                    send_msg = f"{current_state}\n".encode('utf-8')
                    self.client_socket.sendto(send_msg, self.rpi_address)
                    print(f"[YOLO 네트워크] 상태 변경 전송 -> {current_state}")
                except Exception as e:
                    print(f"[YOLO 전송 실패] 데이터 유실 발생: {e}")
            
            # 현재 상태를 과거 상태로 갱신
            self.last_state = current_state
            
        return frame

    def close(self):
        """자원 해제"""
        if self.client_socket:
            self.client_socket.close()