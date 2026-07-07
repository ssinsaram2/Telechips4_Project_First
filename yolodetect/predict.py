from ultralytics import YOLO
import sys

# 1. NCNN 경량화 모델 로드
model = YOLO("yolov8n_ncnn_model")

print("시작하는 중... 종료하려면 영상 창에서 'q'를 누르거나 터미널에서 Ctrl+C를 누르세요.")

try:
    # 2. 웹캠 실시간 추론 스트림 가동
    results_stream = model(source=0, show=True, classes=0, stream=True)
    
    for r in results_stream:
        person_count = len(r.boxes)
        print(f"👥 현재 카메라에 보이는 사람 수: {person_count}명")

# q 키나 Ctrl+C를 눌러서 발생하는 중단 에러(KeyboardInterrupt)를 가로챕니다.
except (KeyboardInterrupt, Exception):
    print("\n🛑 사용자가 종료 요청을 했습니다.")

# 마지막에 무조건 실행되는 블록으로, 시스템을 깨끗하게 정리합니다.
finally:
    print("🧹 카메라 및 그래픽 리소스를 안전하게 해제합니다.")
    print("👋 프로그램이 정상적으로 종료되었습니다.")
    sys.exit(0)