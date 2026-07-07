# recognize.py
import cv2
import dlib
import numpy as np
import os

KNOWN_FACES_DIR = "known_faces"
known_face_encodings = []
known_face_names = []

# 1. dlib의 핵심 컴포넌트들을 직접 선언 (face_recognition 내부 모듈 직접 호출)
detector = dlib.get_frontal_face_detector()
# face_recognition 패키지가 가지고 있는 가중치 모델 파일 경로를 직접 지정합니다.
import face_recognition_models
predictor_path = face_recognition_models.pose_predictor_model_location()
face_rec_path = face_recognition_models.face_recognition_model_location()

sp = dlib.shape_predictor(predictor_path)
facerec = dlib.face_recognition_model_v1(face_rec_path)

def get_face_encoding(img_rgb, dlib_rect):
    """최신 dlib 규격에 맞춰 4개의 인자(padding 포함)를 명시하여 에러를 원천 차단합니다."""
    shape = sp(img_rgb, dlib_rect)
    # 1번 규칙 형태인 (self, img, face, num_jitters, padding) 스펙을 정확히 만족시킵니다.
    face_descriptor = facerec.compute_face_descriptor(img_rgb, shape, 0, 0.25)
    return np.array(face_descriptor)

def load_database():
    """저장된 사진 파일들로부터 얼굴 특징을 추출하여 DB를 구축합니다."""
    print("얼굴 데이터베이스를 로드 중입니다...")
    if not os.path.exists(KNOWN_FACES_DIR):
        print("경고: known_faces 폴더가 없습니다. register.py를 먼저 실행해 주세요.")
        return
        
    for filename in os.listdir(KNOWN_FACES_DIR):
        if filename.endswith(".jpg") or filename.endswith(".png"):
            image_path = os.path.join(KNOWN_FACES_DIR, filename)
            # 이미지 로드 (dlib은 RGB를 사용하므로 OpenCV로 읽어 변환)
            bgr_img = cv2.imread(image_path)
            if bgr_img is None: continue
            rgb_img = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2RGB)
            
            # 얼굴 탐지
            dets = detector(rgb_img, 1)
            if len(dets) > 0:
                encoding = get_face_encoding(rgb_img, dets[0])
                known_face_encodings.append(encoding)
                known_face_names.append(os.path.splitext(filename)[0])
            else:
                print(f"오류: {filename}에서 얼굴을 인식하지 못했습니다.")

    print(f"로드 완료: 총 {len(known_face_names)}명의 얼굴을 식별할 수 있습니다.")

def main():
    load_database()

    cap_v4l2 = cv2.CAP_V4L2
    camera = cv2.VideoCapture(0, cap_v4l2)
    camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    if not camera.isOpened():
        print("에러: 카메라를 열 수 없습니다.")
        return

    print("\n얼굴 식별을 시작합니다. 종료하려면 'q'를 누르세요.")
    process_this_frame = True

    # 화면 표시용 변수 저장
    face_boxes = []
    face_names = []

    while True:
        ret, frame = camera.read()
        if not ret:
            break

        # 연산량 감소를 위해 1/4 크기로 축소 및 RGB 변환
        small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)
        rgb_small_frame = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)

        if process_this_frame:
            # dlib 데텍터로 얼굴 찾기
            dets = detector(rgb_small_frame, 0)
            
            face_boxes = []
            face_names = []
            
            for det in dets:
                # 좌표 백업
                face_boxes.append((det.top(), det.right(), det.bottom(), det.left()))
                
                # 특징 추출
                current_encoding = get_face_encoding(rgb_small_frame, det)
                name = "Unknown"
                
                # 거리 비교 (유사도 측정)
                if len(known_face_encodings) > 0:
                    distances = np.linalg.norm(known_face_encodings - current_encoding, axis=1)
                    best_match_index = np.argmin(distances)
                    
                    # 기준치(tolerance=0.5)보다 거리가 가까우면 그 사람으로 판정
                    if distances[best_match_index] < 0.5:
                        name = known_face_names[best_match_index]
                        
                face_names.append(name)

        process_this_frame = not process_this_frame

        # 결과 시각화
        for (top, right, bottom, left), name in zip(face_boxes, face_names):
            # 1/4 축소본 좌표를 원래 크기(4배)로 복원
            top *= 4; right *= 4; bottom *= 4; left *= 4

            color = (0, 255, 0) if name != "Unknown" else (0, 0, 255)
            cv2.rectangle(frame, (left, top), (right, bottom), color, 2)
            cv2.rectangle(frame, (left, bottom - 35), (right, bottom), color, cv2.FILLED)
            cv2.putText(frame, name, (left + 6, bottom - 6), cv2.FONT_HERSHEY_DUPLEX, 0.8, (255, 255, 255), 1)

        cv2.imshow('Face Recognition System (Pure Dlib)', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    camera.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()