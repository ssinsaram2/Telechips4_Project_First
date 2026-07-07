import time
import cv2
import mediapipe as mp

def main():
    # 1. MediaPipe Face Detection 모듈 초기화
    mp_face_detection = mp.solutions.face_detection
    mp_drawing = mp.solutions.drawing_utils  # 랜드마크 시각화 도구

    # 2. 카메라 캡처 객체 생성 (라즈베리파이에 연결된 0번 카메라)
    cap = cv2.VideoCapture(0)

    # 해상도를 적절히 낮추면 라즈베리파이에서 더 높은 FPS를 확보할 수 있습니다.
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    # [추가] FPS 측정용 변수 (지수 이동 평균으로 흔들림 완화)
    fps_ema = 0.0
    EMA_ALPHA = 0.1
    prev_time = time.perf_counter()

    # 신뢰도 임계값(min_detection_confidence) 설정 (50% 이상일 때 얼굴로 인식)
    with mp_face_detection.FaceDetection(model_selection=0, min_detection_confidence=0.5) as face_detection:
        print("얼굴 인식을 시작합니다. 'q'를 누르면 종료됩니다.")

        while cap.isOpened():
            success, frame = cap.read()
            if not success:
                print("카메라 프레임을 읽을 수 없습니다.")
                break

            # 성능 최적화를 위해 이미지 쓰기 불가능 설정 및 BGR에서 RGB로 변환
            frame.flags.writeable = False
            image_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

            # [핵심 단계] MediaPipe 모델 추론 수행
            results = face_detection.process(image_rgb)

            # 다시 화면에 그리기 위해 쓰기 가능 설정 및 BGR 복원
            frame.flags.writeable = True

            # 얼굴이 검출되었다면 바운딩 박스와 랜드마크(눈, 코, 입 등 6개 점) 시각화
            if results.detections:
                for detection in results.detections:
                    mp_drawing.draw_detection(frame, detection)

            # [추가] FPS 계산 (캡처 + 추론 + 렌더링을 포함한 전체 루프 기준)
            now = time.perf_counter()
            dt = now - prev_time
            prev_time = now
            if dt > 0:
                inst_fps = 1.0 / dt
                fps_ema = inst_fps if fps_ema == 0.0 else (EMA_ALPHA * inst_fps + (1 - EMA_ALPHA) * fps_ema)

            # [추가] 우측 상단에 FPS 표시 (검정 배경 박스 + 노란 글씨)
            fps_text = f"FPS: {fps_ema:.1f}"
            (tw, th), _ = cv2.getTextSize(fps_text, cv2.FONT_HERSHEY_SIMPLEX, 0.7, 2)
            fx = frame.shape[1] - tw - 10
            fy = th + 10
            cv2.rectangle(frame, (fx - 6, fy - th - 6), (fx + tw + 6, fy + 6), (0, 0, 0), -1)
            cv2.putText(frame, fps_text, (fx, fy),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

            # 결과 화면 출력
            cv2.imshow('MediaPipe Face Detection', frame)

            # 'q' 키를 누르면 루프 탈출
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    # 자원 해제
    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()