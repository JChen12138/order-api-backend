FROM gcc:13

# Install packages
RUN apt-get update && apt-get install -y \
    cmake \
    make \
    libsqlite3-dev \
    libhiredis-dev \
    libssl-dev \
    libspdlog-dev \
    libcurl4-openssl-dev \       
    g++ \
    git \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Build and install redis-plus-plus
RUN git clone https://github.com/sewenew/redis-plus-plus.git && \
    cd redis-plus-plus && mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make && make install && cd ../.. && rm -rf redis-plus-plus

# --- Install prometheus-cpp ---
RUN git clone --recurse-submodules https://github.com/jupp0r/prometheus-cpp.git && \
    cd prometheus-cpp && mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DENABLE_PULL=ON && \
    make -j4 && make install && cd ../.. && rm -rf prometheus-cpp

# Set working directory
WORKDIR /app

# Copy your source code
COPY . .

# Build test binary 
RUN g++ -DTEST_API_HOST=\"crow_app\" -std=c++17 -Iinclude -I. test/test_main.cpp test/test_helpers.cpp test/test_endpoints.cpp -o run_tests -lsqlite3 -lfmt -lredis++ -lhiredis


# Build your app
RUN g++ -std=c++17 -O3 -Iinclude src/main.cpp src/order_routes.cpp -o server \
    -lsqlite3 -lredis++ -lhiredis -lpthread -lfmt


# Expose app port
EXPOSE 8080

# Set library path
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Run app
CMD ["./server"]
