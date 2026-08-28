import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:path_provider/path_provider.dart';
import '../services/osm_service.dart';
import '../ffi/native_bridge.dart';

class MapScreen extends StatefulWidget {
  const MapScreen({super.key});

  @override
  State<MapScreen> createState() => _MapScreenState();
}

class _MapScreenState extends State<MapScreen> {
  final MapController _mapController = MapController();
  
  LatLng _center = const LatLng(42.6977, 23.3219);
  double _zoom = 16.0;

  final TextEditingController _minLatController = TextEditingController(text: '42.6950');
  final TextEditingController _minLonController = TextEditingController(text: '23.3180');
  final TextEditingController _maxLatController = TextEditingController(text: '42.7000');
  final TextEditingController _maxLonController = TextEditingController(text: '23.3250');

  TargetPlatformType _selectedPlatform = TargetPlatformType.android;
  bool _enableProps = true;
  bool _isProcessing = false;
  double _progress = 0.0;
  String _statusMessage = 'Изберете зона и натиснете "Генерирай GTA VC Файлове"';
  String? _publicExportPath;

  @override
  void dispose() {
    _minLatController.dispose();
    _minLonController.dispose();
    _maxLatController.dispose();
    _maxLonController.dispose();
    super.dispose();
  }

  void _updateBboxFromMap() {
    final bounds = _mapController.camera.visibleBounds;
    setState(() {
      _minLatController.text = bounds.south.toStringAsFixed(5);
      _minLonController.text = bounds.west.toStringAsFixed(5);
      _maxLatController.text = bounds.north.toStringAsFixed(5);
      _maxLonController.text = bounds.east.toStringAsFixed(5);
    });
  }

  void _showInstallGuideDialog() {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1E1E28),
        title: const Row(
          children: [
            Icon(Icons.sports_esports, color: Color(0xFFFF007F)),
            SizedBox(width: 8),
            Text('Как да пуснеш картата в GTA VC'),
          ],
        ),
        content: const SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('1. Копирай генерираната папка в:', style: TextStyle(fontWeight: FontWeight.bold, color: Colors.cyan)),
              Text('GTA Vice City/data/maps/osm/\n', style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
              Text('2. Отвори data/gta_vc.dat и добави:', style: TextStyle(fontWeight: FontWeight.bold, color: Colors.cyan)),
              SelectableText(
                'CDIMAGE DATA\\MAPS\\OSM\\OSM_MAP.IMG\nIDE DATA\\MAPS\\OSM\\OSM_WORLD.IDE\nIPL DATA\\MAPS\\OSM\\OSM_WORLD.IPL\nCOLFILE 0 DATA\\MAPS\\OSM\\OSM_WORLD.COL',
                style: TextStyle(fontFamily: 'monospace', color: Colors.amber, fontSize: 11),
              ),
              SizedBox(height: 12),
              Text('3. Стартирай играта!', style: TextStyle(fontWeight: FontWeight.bold, color: Colors.greenAccent)),
              Text('Картата ще се зареди автоматично с улични лампи и дървета!'),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('РАЗБРАХ', style: TextStyle(color: Color(0xFFFF007F))),
          ),
        ],
      ),
    );
  }

  Future<void> _startGeneration() async {
    final minLat = double.tryParse(_minLatController.text);
    final minLon = double.tryParse(_minLonController.text);
    final maxLat = double.tryParse(_maxLatController.text);
    final maxLon = double.tryParse(_maxLonController.text);

    if (minLat == null || minLon == null || maxLat == null || maxLon == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Моля, въведете валидни числови координати!')),
      );
      return;
    }

    if ((maxLat - minLat).abs() > 0.05 || (maxLon - minLon).abs() > 0.05) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Зоната е твърде голяма! Изберете по-малък квартал.')),
      );
      return;
    }

    setState(() {
      _isProcessing = true;
      _progress = 0.0;
      _publicExportPath = null;
      _statusMessage = 'Започване на процеса...';
    });

    try {
      final osmFile = await OsmService.downloadOsmArea(
        minLat: minLat,
        minLon: minLon,
        maxLat: maxLat,
        maxLon: maxLon,
        onProgress: (p, msg) {
          setState(() {
            _progress = p;
            _statusMessage = msg;
          });
        },
      );

      final appDir = await getApplicationDocumentsDirectory();
      final folderName = 'GTA_VC_MAP_${DateTime.now().millisecondsSinceEpoch}';
      final internalOutDir = Directory('${appDir.path}/$folderName');
      if (!await internalOutDir.exists()) {
        await internalOutDir.create(recursive: true);
      }

      setState(() {
        _statusMessage = 'C++ Енджинът изгражда 3D свят, лампи, палми и архиви...';
      });

      final native = NativeBridge();
      final success = await native.convertOsmToGta(
        osmFilePath: osmFile.path,
        outputDirPath: internalOutDir.path,
        platform: _selectedPlatform,
        enableProps: _enableProps,
        onProgress: (nativeProg) {
          setState(() {
            _progress = 0.5 + (nativeProg * 0.4);
            _statusMessage = 'C++ Обработка: ${(nativeProg * 100).toInt()}%';
          });
        },
      );

      if (!success) {
        throw Exception('C++ ядрото върна грешка при обработката!');
      }

      setState(() {
        _statusMessage = 'Експортиране на файловете в папка Downloads...';
      });

      Directory publicDownloadDir = Directory('/storage/emulated/0/Download/GTA_VC_Maps/$folderName');
      try {
        if (!await publicDownloadDir.exists()) {
          await publicDownloadDir.create(recursive: true);
        }
      } catch (_) {
        final extDir = await getExternalStorageDirectory();
        publicDownloadDir = Directory('${extDir?.path}/GTA_VC_Maps/$folderName');
        await publicDownloadDir.create(recursive: true);
      }

      int fileCount = 0;
      final generatedFiles = internalOutDir.listSync();
      for (final entity in generatedFiles) {
        if (entity is File) {
          final fileName = entity.uri.pathSegments.last;
          await entity.copy('${publicDownloadDir.path}/$fileName');
          fileCount++;
        }
      }

      _publicExportPath = publicDownloadDir.path;

      setState(() {
        _progress = 1.0;
        _statusMessage = ' Готово! Експортиран населен GTA VC свят ($fileCount файла):\n${publicDownloadDir.path}';
      });

      if (await osmFile.exists()) {
        await osmFile.delete();
      }
    } catch (e) {
      setState(() {
        _statusMessage = 'Грешка: $e';
      });
    } finally {
      setState(() {
        _isProcessing = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('GTA VC Map Builder (OSM)'),
        backgroundColor: const Color(0xFF1E1E28),
        actions: [
          IconButton(
            icon: const Icon(Icons.help_outline),
            tooltip: 'Инструкции за инсталиране',
            onPressed: _showInstallGuideDialog,
          ),
          IconButton(
            icon: const Icon(Icons.crop_free),
            tooltip: 'Вземи координати от екрана',
            onPressed: _isProcessing ? null : _updateBboxFromMap,
          ),
        ],
      ),
      body: Column(
        children: [
          Expanded(
            flex: 3,
            child: FlutterMap(
              mapController: _mapController,
              options: MapOptions(
                initialCenter: _center,
                initialZoom: _zoom,
                onPositionChanged: (pos, hasGesture) {
                  if (pos.center != null) _center = pos.center!;
                  if (pos.zoom != null) _zoom = pos.zoom!;
                },
              ),
              children: [
                TileLayer(
                  urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
                  userAgentPackageName: 'com.stanbg96.gtavc_builder',
                ),
              ],
            ),
          ),
          Expanded(
            flex: 4,
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      const Text('Целева платформа:', style: TextStyle(fontWeight: FontWeight.bold)),
                      SegmentedButton<TargetPlatformType>(
                        segments: const [
                          ButtonSegment(value: TargetPlatformType.pc, label: Text('PC (D3D8)')),
                          ButtonSegment(value: TargetPlatformType.android, label: Text('Android (Mobile)')),
                        ],
                        selected: {_selectedPlatform},
                        onSelectionChanged: _isProcessing
                            ? null
                            : (newSelection) {
                                setState(() {
                                  _selectedPlatform = newSelection.first;
                                });
                              },
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),

                  // Превключвател за лампи и дървета
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('Улични лампи и дървета (Props)', style: TextStyle(fontSize: 14)),
                    subtitle: const Text('Автоматично добавя стълбове и палми в парковете', style: TextStyle(fontSize: 11, color: Colors.white54)),
                    value: _enableProps,
                    activeColor: const Color(0xFFFF007F),
                    onChanged: _isProcessing ? null : (val) => setState(() => _enableProps = val),
                  ),
                  const SizedBox(height: 8),

                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _minLatController,
                          enabled: !_isProcessing,
                          decoration: const InputDecoration(labelText: 'Min Lat (Юг)', border: OutlineInputBorder()),
                          keyboardType: const TextInputType.numberWithOptions(decimal: true),
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: TextField(
                          controller: _minLonController,
                          enabled: !_isProcessing,
                          decoration: const InputDecoration(labelText: 'Min Lon (Запад)', border: OutlineInputBorder()),
                          keyboardType: const TextInputType.numberWithOptions(decimal: true),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _maxLatController,
                          enabled: !_isProcessing,
                          decoration: const InputDecoration(labelText: 'Max Lat (Север)', border: OutlineInputBorder()),
                          keyboardType: const TextInputType.numberWithOptions(decimal: true),
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: TextField(
                          controller: _maxLonController,
                          enabled: !_isProcessing,
                          decoration: const InputDecoration(labelText: 'Max Lon (Изток)', border: OutlineInputBorder()),
                          keyboardType: const TextInputType.numberWithOptions(decimal: true),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  if (_isProcessing) ...[
                    LinearProgressIndicator(
                      value: _progress > 0 ? _progress : null,
                      color: const Color(0xFFFF007F),
                      backgroundColor: Colors.white12,
                    ),
                    const SizedBox(height: 8),
                  ],
                  Container(
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: const Color(0xFF282836),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(
                        color: _publicExportPath != null ? const Color(0xFF00F0FF) : Colors.transparent,
                      ),
                    ),
                    child: Column(
                      children: [
                        Text(
                          _statusMessage,
                          style: const TextStyle(fontSize: 13, color: Colors.white70),
                          textAlign: TextAlign.center,
                        ),
                        if (_publicExportPath != null) ...[
                          const SizedBox(height: 10),
                          Row(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              ElevatedButton.icon(
                                onPressed: () {
                                  Clipboard.setData(ClipboardData(text: _publicExportPath!));
                                  ScaffoldMessenger.of(context).showSnackBar(
                                    const SnackBar(content: Text('Пътят е копиран!')),
                                  );
                                },
                                icon: const Icon(Icons.copy, size: 16),
                                label: const Text('Копирай път'),
                                style: ElevatedButton.styleFrom(
                                  backgroundColor: const Color(0xFF00F0FF),
                                  foregroundColor: Colors.black,
                                ),
                              ),
                              const SizedBox(width: 8),
                              ElevatedButton.icon(
                                onPressed: _showInstallGuideDialog,
                                icon: const Icon(Icons.info_outline, size: 16),
                                label: const Text('Инструкции'),
                                style: ElevatedButton.styleFrom(
                                  backgroundColor: const Color(0xFFFF007F),
                                  foregroundColor: Colors.white,
                                ),
                              ),
                            ],
                          ),
                        ],
                      ],
                    ),
                  ),
                  const SizedBox(height: 12),
                  ElevatedButton.icon(
                    onPressed: _isProcessing ? null : _startGeneration,
                    icon: const Icon(Icons.build_circle),
                    label: Text(_isProcessing ? 'ГЕНЕРИРАНЕ...' : 'ГЕНЕРИРАЙ GTA VC ФАЙЛОВЕ'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: const Color(0xFFFF007F),
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 14),
                      textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
