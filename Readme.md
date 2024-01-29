# C++ DPF-PIR library

This is a high-performance implementation of private information retrieval (PIR) 
based on distributed point functions (DPF). As this is a multi-server 
(more specifically a 2-server) PIR, we need two *non-colluding* servers 
with identical databases. For our use case, where this DPF-PIR is being used in a
Certificate Transparency (CT) log server, we specialize on databases of SHA-256 hash
values, which are stored in a *hashdatastore* object. This aligns nicely with the use
of AVX/AVX2 instructions to speed up calculations. We also make use of AES-NI instructions
to create a high-performance PRF.


## Executing Microbenchmarks
Example execution of microbenchmarks for a tree with 2^22 elements.

```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./dpf_pir 22
```# Implementation for the paper: High-Throughput Three-Party DPFs with Applications to ORAM and Digital Currencies

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


## References

Some parts like the AES-NI implementation are taken from Peter Rindal's public domain [CryptoTools](https://github.com/ladnir/cryptoTools/).

[Paper](http://www.ramacher.at/_static/papers/ct-privacy.pdf): *Daniel Kales, Olamide Omolola, Sebastian Ramacher*. **Revisting User Privacy for Certificate Transparency**. EuroS&P 2019
