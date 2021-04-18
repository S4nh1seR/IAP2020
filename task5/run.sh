python ./calc_feature_repeatability.py -fp ./src_frames -rp ./results -dt SIFT
python ./calc_feature_repeatability.py -fp ./src_frames -rp ./results -dt BRISK
python ./calc_feature_repeatability.py -fp ./src_frames -rp ./results -dt AKAZE
python ./calc_feature_repeatability.py -fp ./src_frames -rp ./results -dt SHI_TOMASI
python ./union_ratio_plots.py -rp ./results
