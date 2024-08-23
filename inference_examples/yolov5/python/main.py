import cv2
import time
import numpy as np
import ff_pymedia as m
from yolov5 import inf_task

video_path = "./sample_720p.mp4"
model_path = "../model/RK3588/yolov5s-640-640.rknn"
output_path = "./result.mp4"

class inferenceObj():
    def __init__(self, name, module):
        self.name = name
        self.inf_module = module
        self.ratio_w = 1.0
        self.ratio_h = 1.0


def call_back_func(obj, media_Buffer):
    img = inf_task(obj.inf_module, media_Buffer, (obj.ratio_w, obj.ratio_h))

    cv2.imshow(obj.name, img)
    cv2.waitKey(1)

def main():

    input_source = m.ModuleFileReader(video_path, False)
    ret = input_source.init()
    if ret < 0:
        print("Failed to init file reader")
        return 1
    
    # Create the decoder module
    dec_module = m.ModuleMppDec()
    dec_module.setProductor(input_source) # Join the source module consumer queue.
    ret = dec_module.init()
    if ret < 0:
        print("Failed to init decoder")
        return 1
    
    # Create the rga module
    output_image_param = dec_module.getOutputImagePara()
    output_image_param.v4l2Fmt = m.v4l2GetFmtByName("BGR24")
    rga_modele = m.ModuleRga(output_image_param, m.RgaRotate.RGA_ROTATE_NONE)
    rga_modele.setProductor(dec_module) # Join the decoder module consumer queue.
    rga_modele.setBufferCount(2)
    ret = rga_modele.init()
    if ret < 0:
        print("Failed to init rga")
        return 1
    
    # Create the inference module
    # The inference module is used for manual inference and is not added to the module link.
    inf_input_image_param = rga_modele.getOutputImagePara()
    inf_obj = inferenceObj("inference test", None)
    inf_obj.inf_module = m.ModuleInference(inf_input_image_param)
    inf_obj.inf_module.setModelData(model_path, 0) # Set the model file
    ret = inf_obj.inf_module.init()
    if ret < 0:
        print("Failed to init inferencer module")
        return 1
    
    inf_obj.ratio_w = inf_obj.inf_module.getOutputImageCrop().w / inf_input_image_param.width
    inf_obj.ratio_h = inf_obj.inf_module.getOutputImageCrop().h / inf_input_image_param.height
    rga_modele.setOutputDataCallback(inf_obj, call_back_func)
    
    # Create the encoder module
    enc_module = m.ModuleMppEnc(m.EncodeType.ENCODE_TYPE_H264)
    enc_module.setProductor(rga_modele) # Join the rga module consumer queue.
    ret = enc_module.init()
    if ret < 0:
        print("Failed to init encoder")
        return 1
    
    # Create the file writer module
    file_writer = m.ModuleFileWriter(output_path)
    file_writer.setProductor(enc_module) # Join the encoder module consumer queue.
    ret = file_writer.init()
    if ret < 0:
        print("Failed to init file writer")
        return 1

    input_source.dumpPipe()
    input_source.start()
    input("wait...")
    input_source.stop()
    input_source.dumpPipeSummary()

if __name__ == "__main__":
    main()