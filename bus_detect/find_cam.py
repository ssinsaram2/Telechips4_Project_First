import cv2

def scan_cameras():
    print("🔍 라즈베리 파이 5 내부에서 작동 가능한 진짜 비디오 스트림 번호를 스캔합니다...")
    valid_indices = []
    
    for index in range(30):
        # 라파5 환경에서 안전하게 열기 위해 CAP_V4L2를 지정합니다.
        cap = cv2.VideoCapture(index, cv2.CAP_V4L2)
        if cap.isOpened():
            ret, frame = cap.read()
            if ret and frame is not None:
                print(f"✅ [성공] 인덱스 {index}번은 진짜 카메라 화면입니다! (해상도: {frame.shape[1]}x{frame.shape[0]})")
                valid_indices.append(index)
            cap.release()
            
    if not valid_indices:
        print("❌ 실패: 사용 가능한 진짜 카메라 번호를 찾지 못했습니다. USB 연결을 확인하세요.")
    else:
        print(f"🎉 스캔 완료! 찾은 카메라 번호 목록: {valid_indices}")

if __name__ == "__main__":
    scan_cameras()