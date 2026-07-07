# recognize_async.py
# 개선 포인트:
#   1) 캡처 스레드: 카메라에서 계속 읽어 "최신 프레임"만 유지 (버퍼 밀림 방지)
#   2) 인식 워커 스레드: 무거운 dlib 검출+인코딩을 화면 표시와 분리
#      -> 화면은 카메라 속도(~30 FPS)로 계속 갱신되고,
#         박스/이름은 인식이 끝날 때마다 비동기로 따라붙음
#   3) 우측 상단 FPS 표시: 화면 FPS와 인식 FPS를 각각 표시
import cv2
import dlib
import numpy as np
import os
import time
import threading

KNOWN_FACES_DIR = "known_faces"
known_face_encodings = []
known_face_names = []

detector = dlib.get_frontal_face_detector()
import face_recognition_models
predictor_path = face_recognition_models.pose_predictor_model_location()
face_rec_path = face_recognition_models.face_recognition_model_location()

sp = dlib.shape_predictor(predictor_path)
facerec = dlib.face_recognition_model_v1(face_rec_path)

DOWNSCALE = 0.25          # 인식용 축소 비율 (원본 대비)
TOLERANCE = 0.5           # dlib 유클리드 거리 임계값


def get_face_encoding(img_rgb, dlib_rect):
    shape = sp(img_rgb, dlib_rect)
    face_descriptor = facerec.compute_face_descriptor(img_rgb, shape, 0, 0.25)
    return np.array(face_descriptor)


def load_database():
    print("얼굴 데이터베이스를 로드 중입니다...")
    if not os.path.exists(KNOWN_FACES_DIR):
        print("경고: known_faces 폴더가 없습니다. register.py를 먼저 실행해 주세요.")
        return
    for filename in os.listdir(KNOWN_FACES_DIR):
        if filename.endswith(".jpg") or filename.endswith(".png"):
            bgr_img = cv2.imread(os.path.join(KNOWN_FACES_DIR, filename))
            if bgr_img is None:
                continue
            rgb_img = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2RGB)
            dets = detector(rgb_img, 1)
            if len(dets) > 0:
                known_face_encodings.append(get_face_encoding(rgb_img, dets[0]))
                known_face_names.append(os.path.splitext(filename)[0])
            else:
                print(f"오류: {filename}에서 얼굴을 인식하지 못했습니다.")
    print(f"로드 완료: 총 {len(known_face_names)}명의 얼굴을 식별할 수 있습니다.")


def main():
    load_database()
    encodings_np = np.array(known_face_encodings) if known_face_encodings else None

    camera = cv2.VideoCapture(0, cv2.CAP_V4L2)
    camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    if not camera.isOpened():
        print("에러: 카메라를 열 수 없습니다.")
        return

    print("\n얼굴 식별을 시작합니다. 종료하려면 'q'를 누르세요.")

    # ---------- 공유 상태 ----------
    latest_frame = None          # 캡처 스레드가 계속 덮어쓰는 최신 프레임
    frame_lock = threading.Lock()

    results = ([], [])           # (face_boxes, face_names) — 인식 워커가 갱신
    results_lock = threading.Lock()

    infer_fps = 0.0              # 인식 파이프라인 FPS (워커가 갱신)
    running = True

    # ---------- 1) 캡처 전용 스레드 ----------
    def capture_thread():
        nonlocal latest_frame
        while running and camera.isOpened():
            ret, frame = camera.read()
            if not ret:
                break
            with frame_lock:
                latest_frame = frame

    # ---------- 2) 인식 전용 워커 스레드 ----------
    def infer_thread():
        nonlocal infer_fps
        ema = 0.0
        while running:
            with frame_lock:
                frame = None if latest_frame is None else latest_frame.copy()
            if frame is None:
                time.sleep(0.005)
                continue

            t0 = time.perf_counter()

            small = cv2.resize(frame, (0, 0), fx=DOWNSCALE, fy=DOWNSCALE)
            rgb_small = cv2.cvtColor(small, cv2.COLOR_BGR2RGB)

            dets = detector(rgb_small, 0)
            boxes, names = [], []
            for det in dets:
                boxes.append((det.top(), det.right(), det.bottom(), det.left()))
                name = "Unknown"
                if encodings_np is not None:
                    enc = get_face_encoding(rgb_small, det)
                    distances = np.linalg.norm(encodings_np - enc, axis=1)
                    idx = int(np.argmin(distances))
                    if distances[idx] < TOLERANCE:
                        name = known_face_names[idx]
                names.append(name)

            with results_lock:
                nonlocal_results = (boxes, names)
                results_holder[0] = nonlocal_results

            dt = time.perf_counter() - t0
            if dt > 0:
                inst = 1.0 / dt
                ema = inst if ema == 0.0 else (0.2 * inst + 0.8 * ema)
                infer_fps = ema

    # 파이썬 클로저에서 튜플 재할당을 위해 리스트 홀더 사용
    results_holder = [results]

    t_cap = threading.Thread(target=capture_thread, daemon=True)
    t_inf = threading.Thread(target=infer_thread, daemon=True)
    t_cap.start()
    t_inf.start()

    # ---------- 3) 메인(표시) 루프: 카메라 속도로 계속 렌더링 ----------
    scale = int(round(1.0 / DOWNSCALE))
    disp_fps = 0.0
    prev = time.perf_counter()

    while True:
        with frame_lock:
            frame = None if latest_frame is None else latest_frame.copy()
        if frame is None:
            time.sleep(0.005)
            continue

        with results_lock:
            face_boxes, face_names = results_holder[0]

        for (top, right, bottom, left), name in zip(face_boxes, face_names):
            top *= scale; right *= scale; bottom *= scale; left *= scale
            color = (0, 255, 0) if name != "Unknown" else (0, 0, 255)
            cv2.rectangle(frame, (left, top), (right, bottom), color, 2)
            cv2.rectangle(frame, (left, bottom - 35), (right, bottom), color, cv2.FILLED)
            cv2.putText(frame, name, (left + 6, bottom - 6),
                        cv2.FONT_HERSHEY_DUPLEX, 0.8, (255, 255, 255), 1)

        # ----- FPS 계산 (표시 루프 기준) -----
        now = time.perf_counter()
        dt = now - prev
        prev = now
        if dt > 0:
            inst = 1.0 / dt
            disp_fps = inst if disp_fps == 0.0 else (0.1 * inst + 0.9 * disp_fps)

        # ----- 우측 상단 FPS 오버레이 (화면 FPS / 인식 FPS 각각 표시) -----
        line1 = f"Display: {disp_fps:.1f} FPS"
        line2 = f"Infer:   {infer_fps:.1f} FPS"
        font = cv2.FONT_HERSHEY_SIMPLEX
        (w1, h1), _ = cv2.getTextSize(line1, font, 0.6, 2)
        (w2, h2), _ = cv2.getTextSize(line2, font, 0.6, 2)
        tw = max(w1, w2)
        fx = frame.shape[1] - tw - 12
        cv2.rectangle(frame, (fx - 8, 6), (frame.shape[1] - 4, 6 + h1 + h2 + 22), (0, 0, 0), -1)
        cv2.putText(frame, line1, (fx, 6 + h1 + 6), font, 0.6, (0, 255, 255), 2)
        cv2.putText(frame, line2, (fx, 6 + h1 + h2 + 16), font, 0.6, (0, 255, 255), 2)

        cv2.imshow('Face Recognition System (Async Dlib)', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    running = False
    time.sleep(0.05)
    camera.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()