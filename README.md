# LLVM Dataflow Analysis

Implement classical data flow algorithm via LLVM. Currently I completed:
- Reaching Definition
- Liveness Analysis
- Available Expression Analysis

# Intallation

Export your LLVM install directory to `$LLVM_HOME`.

Then 
```
mkdir build && cd build
cmake ../
make
```
