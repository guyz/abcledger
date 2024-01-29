# Implementation for the paper: High-Throughput Three-Party DPFs with Applications to ORAM and Digital Currencies

Implementation of our DPF protocols (see dpf.h), ORAM and CBDC (see Server.h).

## Executing Benchmarks

Build, then run as server 0, 1 and 2 with the desired benchmark.

```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./prioram <server_index> <log_db_size> <benchmarks>
```
To set network parameters, use:

```
sudo tc qdisc add dev lo root netem delay <delay>ms rate <throughput>mbit
```

## List of Benchmarks

* local // runs all DPF tests (no networking)
* DPF.Gen
* DPF.EvalAll
* ShamirDPF.Gen
* ShamirDPF.EvalAll
* VerShamirDPF.Gen
* VerShamirDPF.EvalAll
* ShamirDPFMulti.EvalAll
* FastDPF.EvalAll
* balance
* balanceMalicious
* transfer
* transferMalicious
* read
* write


## References
* DPF-library basis is take from https://github.com/dkales/dpf-cpp and the paper: [Paper](http://www.ramacher.at/_static/papers/ct-privacy.pdf)
