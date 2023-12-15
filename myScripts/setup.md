## Steps
1. Setup xgboost, install dependencies, and build libCacheSim
2. Generate 4-day trace data using extract_trace_4_days.py
3. Copy the corresponding run_<trace_number>.sh script to /users/mthevend/mnt/CloudStorage/midhush/libCacheSim/_build, and run it
    - This generates the bytes missed (for the configuration specified in the trace file) in the results folder in _build
4. Determine the cost using calculate_cost.py
5. Plot graphs using plot_cost.py and plot_min_cost.py

### Setup xgboost
```
sudo mkdir /disk
cd /disk

sudo bash
export PATH=/users/mthevend/mnt/CloudStorage/midhush/cmake-3.27.7/bin:$PATH

git clone --recursive https://github.com/dmlc/xgboost;
pushd xgboost;
mkdir _build && cd _build;
cmake .. && make -j; 
sudo make install;
popd;

exit
```

### Install dependencies and Build libCacheSim
```
cd /users/mthevend/mnt/CloudStorage/midhush/libCacheSim/scripts/
export PATH=/users/mthevend/mnt/CloudStorage/midhush/cmake-3.27.7/bin:$PATH

bash install_dependency.sh && bash install_libcachesim.sh

cd ../
rm -rf _build
mkdir _build && cd _build;
cmake -DENABLE_GLCACHE=1 -DENABLE_LRB=1 .. && make -j;
```