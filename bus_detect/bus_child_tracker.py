import cv2

class BusChildTrackerModule:
    def __init__(self, yolo_model, recognizer):
        self.yolo_model = yolo_model
        self.recognizer = recognizer
        self.inside_bus_children = {}
        self.disembarked_tracks = set()
        self.ENTRY_EXIT_LINE_X = 500

    def process_frame(self, frame):
        results = self.yolo_model.track(
            source=frame, show=False, classes=0, persist=True, 
            tracker="bytetrack.yaml", conf=0.4, iou=0.5, verbose=False
        )
        r = results[0]
        
        if r.boxes.id is not None:
            track_ids = r.boxes.id.int().tolist()
            boxes = r.boxes.xyxy.int().tolist()
            
            for t_id, box in zip(track_ids, boxes):
                x1, y1, x2, y2 = box
                center_x = (x1 + x2) / 2
                
                # A. 기준선 왼쪽 (버스 내부 영역)
                if center_x < self.ENTRY_EXIT_LINE_X:
                    if t_id in self.disembarked_tracks: 
                        self.disembarked_tracks.discard(t_id)
                        print(f"🔄 Re-entry detected for ID:{t_id}")

                    # [교정 포인트 1] self. 추가
                    if t_id not in self.inside_bus_children:
                        self.inside_bus_children[t_id] = {"name": "Unknown", "role": "unknown", "face_identified": False}
                    
                    # [교정 포인트 2] self. 추가
                    if not self.inside_bus_children[t_id]["face_identified"]:
                        h, w, _ = frame.shape
                        x1_c, y1_c, x2_c, y2_c = max(0, x1), max(0, y1), min(w, x2), min(h, y2)
                        person_crop = frame[y1_c:y2_c, x1_c:x2_c]
                        name, role = self.recognizer.identify_face(person_crop)
                        
                        if name != "Unknown":
                            if role == "driver":
                                # [교정 포인트 3] self. 추가
                                if t_id in self.inside_bus_children: del self.inside_bus_children[t_id]
                                continue
                            # [교정 포인트 4] self. 추가
                            self.inside_bus_children[t_id] = {"name": name, "role": role, "face_identified": True}
                    
                    curr_info = self.inside_bus_children.get(t_id, {"name": "Unknown", "role": "unknown"})
                    color = (0, 255, 0)
                    display_name = curr_info["name"] if curr_info["name"] != "Unknown" else "Unregistered"
                    label = f"ID:{t_id} {display_name}"
                    cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                    cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
                    
                # B. 기준선 오른쪽 (하차 영역)
                elif center_x >= self.ENTRY_EXIT_LINE_X:
                    if t_id in self.disembarked_tracks: continue
                    if t_id in self.inside_bus_children:
                        left_info = self.inside_bus_children.pop(t_id)
                        if left_info["role"] != "driver":
                            self.disembarked_tracks.add(t_id)
                            target_name = left_info["name"]
                            if target_name != "Unknown":
                                print(f"👋 [Disembarked] {target_name} left the bus.")
                                ghost_keys = [k for k, v in self.inside_bus_children.items() if v["name"] == target_name]
                                for gk in ghost_keys: self.inside_bus_children.pop(gk, None)
                            else:
                                print(f"👋 [Disembarked] Unregistered person left.")

        cv2.line(frame, (self.ENTRY_EXIT_LINE_X, 0), (self.ENTRY_EXIT_LINE_X, frame.shape[0]), (0, 0, 255), 2)
        
        # 중복 제거 명단 계산
        raw_children_names = [info["name"] for info in self.inside_bus_children.values() if info["role"] == "child"]
        unique_children_names = list(set(raw_children_names))
        actual_children_count = len(unique_children_names)
        
        if actual_children_count > 0:
            print(f"🚌 [Inside]: {', '.join(unique_children_names)} (Total: {actual_children_count})", end="\r")
            cv2.putText(frame, f"Names: {', '.join(unique_children_names)}", (10, frame.shape[0] - 80), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
        else:
            print("🚌 [Inside]: None (Total: 0)                                      ", end="\r")
            
        cv2.putText(frame, f"Inside Children: {actual_children_count}", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
        return frame