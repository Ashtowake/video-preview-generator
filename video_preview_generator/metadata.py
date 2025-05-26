import os
import cv2
from datetime import timedelta

def get_video_metadata(path: str) -> dict:
    """Return filename and duration (HH:MM:SS) for a video file."""
    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        return {"filename": os.path.basename(path), "duration": "00:00:00"}
    fps = cap.get(cv2.CAP_PROP_FPS) or 0
    count = cap.get(cv2.CAP_PROP_FRAME_COUNT) or 0
    cap.release()
    seconds = count / fps if fps else 0
    td = timedelta(seconds=int(seconds))
    hh, mm, ss = td.seconds // 3600, (td.seconds % 3600) // 60, td.seconds % 60
    return {"filename": os.path.basename(path), "duration": f"{hh:02d}:{mm:02d}:{ss:02d}"}
