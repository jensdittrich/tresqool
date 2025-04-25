To reproduce the results shown in the paper, do the following:

(1.) Compile "benchmark.cpp":
    clang++ -std=c++20 -O2 -o build/benchmark benchmark.cpp

    -> This will create the binary "benchmark" in directory "build"

(2.) Run the benchmark:
    ./build/benchmark

    -> This will create two result files "results.csv" and "results_build.csv" in directory "results"

(3.) Start the jupyter server:
    jupyter notebook

    -> This will start a jupyter server in the current directory. If you do not have jupyter installed, see https://jupyter.org/install

(4.) Open the jupyter notebook "Visualization.ipynb" and run all cells:

    -> This will create four different result_*.pdf files in directory "../../"
    -> These pdfs are the ones included in the paper.
