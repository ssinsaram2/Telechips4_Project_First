# register.py
import cv2
import os

KNOWN_FACES_DIR = "known_faces"
if not os.path.exists(KNOWN_FACES_DIR):
    os.makedirs(KNOWN_FACES_DIR)

def main():
    cap_v4l2 = cv2.CAP_V4L2
    camera = cv2.VideoCapture(0, cap_v4l2)

    if not camera.isOpened():
        print("에러: 카메라를 열 수 없습니다.")
        return

    print("\n[얼굴 등록 가이드]")
    print("'s' 키: 현재 화면을 캡처하여 이름으로 등록")
    print("'q' 키: 등록 종료\n")

    while True:
        ret, frame = camera.read()
        if not ret:
            print("프레임을 가져올 수 없습니다.")
            break

        # 등록용 화면 표시
        cv2.imshow('Face Registration - Press [s] to Save', frame)

        key = cv2.waitKey(1) & 0xFF

        if key == ord('s'):
            print("\n[캡처] 등록할 사람의 이름을 영문으로 입력하세요.")
            name = input("이름 입력 (예: Hong): ").strip()
            
            if name:
                filename = os.path.join(KNOWN_FACES_DIR, f"{name}.jpg")
                if os.path.exists(filename):
                    print(f"오류: '{name}'은(는) 이미 등록된 이름입니다.")
                else:
                    # 박스나 글씨가 없는 깨끗한 원본 프레임을 저장
                    cv2.imwrite(filename, frame)
                    print(f"성공: '{name}'님의 사진이 저장되었습니다. ({filename})")
            else:
                print("이름이 입력되지 않아 취소합니다.")

        elif key == ord('q'):
            break

    camera.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()