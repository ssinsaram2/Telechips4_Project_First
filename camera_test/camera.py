import cv2

def main():
    # 1. 카메라 객체 생성 (0번은 /dev/video0을 의미합니다)
    # 라즈베리파이/우분투 환경에서는 cv2.CAP_V4L2를 명시해주는 것이 안정적입니다.
    cap = cv2.CAP_V4L2
    camera = cv2.VideoCapture(0, cap)

    # 카메라가 정상적으로 열렸는지 확인
    if not camera.isOpened():
        print("에러: 카메라를 열 수 없습니다. (/dev/video0 확인 필요)")
        return

    print("카메라가 성공적으로 연결되었습니다. 종료하려면 'q'를 누르세요.")

    # 2. 실시간 프레임 루프 시작
    while True:
        # 카메라로부터 한 프레임 읽기
        ret, frame = camera.read()

        # 프레임을 정상적으로 읽지 못했다면 루프 탈출
        if not ret:
            print("프레임을 가져올 수 없습니다.")
            break

        # 화면에 영상 표시
        cv2.imshow('Raspberry Pi 5 USB Camera', frame)

        # 키보드 입력 대기 (1ms 대기 후 'q'가 눌렸는지 확인)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    # 3. 자원 해제 및 윈도우 닫기
    camera.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()