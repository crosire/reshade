## A Simple example on sharing Easylogging++ storage

```
.
├── compile_shared.sh
├── lib
│   ├── include
│   │   ├── easylogging++.h
│   │   └── mylib.hpp
│   └── mylib.cpp
├── myapp.cpp
└── README.md

2 directories, 6 files
```

### What is storage

A storage is internal concept. It is main entry class for picking data from e.g, loggers, configurations etc.

### Why do I need to share storage

In a normal application, you should not need to share storage but there will be cases where you would need it, some reasons are as follow

 * In `mylib.cpp` we have registered a logger `mylib` and in `myapp.cpp`, we are using this logger without having to register it again. Try and replace `_SHARE_EASYLOGGINGPP(MyLib::getEasyloggingStorage())` with `_INITIALIZE_EASYLOGGINGPP` (this will initialize a new storage for your app) and run the sample, you would notice a log line saying `Logger [mylib] is not registered yet!`.
 * Saves memory by not doubling up storage when you can share existing one.
 * In extension based project, you would require to share, even sometimes initialize a null storage i.e, using `_INITIALIZE_NULL_EASYLOGGINGPP`. (See [Project Islam](https://github.com/mkhan3189/project-islam/blob/master/extensions/al-quran/al_quran.cc) for a sample (you also may be interested in [this file](https://github.com/mkhan3189/project-islam/blob/master/core/extension/extension_base.h#L152).))
