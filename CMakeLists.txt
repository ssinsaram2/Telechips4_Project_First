cmake_minimum_required(VERSION 3.16)
project(telechips_project_qt6 LANGUAGES C CXX)

# 통합 빌드 루트.
#
# - rpi4_central: CAN 수신, 상태머신, RPi5 UDP 제어, Qt UI IPC 송신
# - qt6_ui      : POSIX mqueue/UDP 기반 Qt6 화면
#
# 개별 빌드도 가능하지만, 프로젝트 루트에서 cmake를 실행하면 두 타겟을 함께 빌드한다.

add_subdirectory(rpi4_central)
add_subdirectory(qt6_ui)
