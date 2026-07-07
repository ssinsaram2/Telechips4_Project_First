import cv2
from insightface.app import FaceAnalysis

# 1. 엔진 초기화
app = FaceAnalysis(providers=['CPUExecutionProvider'])
app.prepare(ctx_id=0, det_size=(640, 640))

# 2. 실제 이미지 로드 (폴더 안에 face.jpg가 있어야 합니다)
img = cv2.imread("HHJ.jpg")

# 3. 얼굴 분석 수행
faces = app.get(img)

# 4. 결과 출력
print(f"검출된 얼굴 수: {len(faces)}")

for i, face in enumerate(faces):
    # 성별과 나이 예측 결과 추출
    gender = "남성" if face.sex == "M" else "여성"
    age = face.age
    print(f"[얼굴 {i+1}] 성별: {gender}, 나이: {int(age)}세")