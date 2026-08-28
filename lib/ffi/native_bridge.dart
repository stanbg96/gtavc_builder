import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';

typedef NativeProcessOsmFunc = Int32 Function(
    Pointer<Utf8> osmPath,
    Pointer<Utf8> outDir,
    Int32 targetPlatform,
    Pointer<NativeFunction<Void Function(Float)>> callback);

typedef DartProcessOsmFunc = int Function(
    Pointer<Utf8> osmPath,
    Pointer<Utf8> outDir,
    int targetPlatform,
    Pointer<NativeFunction<Void Function(Float)>> callback);

enum TargetPlatformType {
  pc(0),
  android(1);

  final int value;
  const TargetPlatformType(this.value);
}

class NativeBridge {
  static final NativeBridge _instance = NativeBridge._internal();
  factory NativeBridge() => _instance;

  late DynamicLibrary _dylib;
  late DartProcessOsmFunc _processOsm;

  NativeBridge._internal() {
    if (Platform.isAndroid) {
      _dylib = DynamicLibrary.open('libgtavc_engine.so');
    } else if (Platform.isWindows) {
      _dylib = DynamicLibrary.open('gtavc_engine.dll');
    } else if (Platform.isLinux) {
      _dylib = DynamicLibrary.open('libgtavc_engine.so');
    } else if (Platform.isMacOS || Platform.isIOS) {
      _dylib = DynamicLibrary.process();
    } else {
      throw UnsupportedError('Неподдържана платформа за FFI');
    }

    _processOsm = _dylib
        .lookup<NativeFunction<NativeProcessOsmFunc>>('ProcessOsmData')
        .asFunction<DartProcessOsmFunc>();
  }

  static Function(double)? _currentProgressCallback;

  static void _nativeCallback(double progress) {
    _currentProgressCallback?.call(progress);
  }

  Future<bool> convertOsmToGta({
    required String osmFilePath,
    required String outputDirPath,
    required TargetPlatformType platform,
    required Function(double progress) onProgress,
  }) async {
    _currentProgressCallback = onProgress;

    final osmPathPtr = osmFilePath.toNativeUtf8();
    final outDirPtr = outputDirPath.toNativeUtf8();

    final nativeCallable = NativeCallable<Void Function(Float)>.isolateLocal(
      _nativeCallback,
      exceptionalReturn: null,
    );

    try {
      final result = _processOsm(
        osmPathPtr,
        outDirPtr,
        platform.value,
        nativeCallable.nativeFunction,
      );
      return result == 0;
    } finally {
      calloc.free(osmPathPtr);
      calloc.free(outDirPtr);
      nativeCallable.close();
      _currentProgressCallback = null;
    }
  }
}
