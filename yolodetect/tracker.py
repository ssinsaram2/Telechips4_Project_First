from ultralytics import YOLO
import sys

model = YOLO("yolov8n_ncnn_model")

# [핵심 상태 관리 변수들]
inside_bus_children = set()  # 현재 버스 내부에 있는 아이들 ID
disembarked_tracks = set()   # 이미 하차 완료하여 다시 카운트하지 않을 아이들 ID (중복 방지 블랙리스트)

# 세로 하차 기준선 (예: 카메라 가로 해상도가 640일 때, 오른쪽 500 위치에 가상의 세로선 설정)
# 실제 버스 출입문 위치에 맞게 이 X 좌표를 조절하세요.
ENTRY_EXIT_LINE_X = 500 

try:
    # iou=0.5, conf=0.4 옵션을 추가하여 박스가 겹치거나 쪼개져서 두 번 나오는 현상을 1차적으로 억제합니다.
    results_stream = model.track(
        source=0, 
        show=True, 
        classes=0, 
        stream=True, 
        persist=True, 
        tracker="bytetrack.yaml",
        conf=0.4,  # 확신도가 너무 낮은 박스는 버림
        iou=0.5    # 박스가 겹쳐서 두 개 생기는 이중 검출 방지 (Non-Maximum Suppression)
    )
    
    for r in results_stream:
        if r.boxes.id is not None:
            track_ids = r.boxes.id.int().tolist()
            boxes = r.boxes.xyxy.int().tolist()
            
            for t_id, box in zip(track_ids, boxes):
                # 이미 완벽하게 하차 처리된 ID라면 중복 연산을 하지 않고 패스 (중복 방지 핵심)
                if t_id in disembarked_tracks:
                    continue
                
                # 아이의 중심점 X 좌표 계산
                center_x = (box[0] + box[2]) / 2
                
                # 1. 기준선 왼쪽(버스 내부)에 있으면 탑승 목록에 유지/추가
                if center_x < ENTRY_EXIT_LINE_X:
                    inside_bus_children.add(t_id)
                
                # 2. 기준선 오른쪽(출입문 바깥 영역)으로 넘어가는 순간 하차 처리
                elif center_x >= ENTRY_EXIT_LINE_X:
                    if t_id in inside_bus_children:
                        inside_bus_children.remove(t_id)
                    
                    # 하차 완료 블랙리스트에 등록하여 이 ID로 인한 두 번 감지를 원천 차단
                    disembarked_tracks.add(t_id)
                    print(f"👋 [하차 확정] ID {t_id}번 아동이 오른쪽 출입문을 통해 하차했습니다.")

        # 최종 잔류 의심 인원 출력
        print(f"🚨 [안전 점검] 현재 버스 내 잔류 의심 아동 수: {len(inside_bus_children)}명 (ID 목록: {list(inside_bus_children)})")

except (KeyboardInterrupt, Exception):
    print("\n🛑 사용자가 종료 요청을 했습니다.")
finally:
    sys.exit(0)