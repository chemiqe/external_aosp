# Android exteranal programs
## test_camera
 Open camera and dump buffers to the file.
 - d : select device (default /dev/video0)
 - s : sleep between each of the ioctls (for testing)
 - n : number of frames (default: 1)

 ### Run:
 run in /data/data

 ### Run movie in x86
 ```
ffplay -f rawvideo -pixel_format rgb24 -video_size 1280x720 frame.raw 
```


## egl_image_test
Test allocation of the buffors using eglCreateImageKHR