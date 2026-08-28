import 'dart:io';
import 'package:flutter/material.dart';
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
  bool _isProcessing = false;
  double _progress = 0.0;
  String _statusMessage = 'Изберете зона и натиснете "Генерирай GTA VC Файлове"';

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
        const SnackBar(content: Text('Зоната е твърде голяма! Моля, изберете по-малък регион.')),
      );
      return;
    }

    setState(() {
      _isProcessing = true;
      _progress = 0.0;
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
      final outDir = Directory('${appDir.path}/GTA_VC_OUTPUT_${DateTime.now().millisecondsSinceEpoch}');
      if (!await outDir.exists()) {
        await outDir.create(recursive: true);
      }

      setState(() {
        _statusMessage = 'C++ Енджинът генерира 3D геометрия (.dff, .txd, .col)...';
      });

      final native = NativeBridge();
      final success = await native.convertOsmToGta(
        osmFilePath: osmFile.path,
        outputDirPath: outDir.path,
        platform: _selectedPlatform,
        onProgress: (nativeProg) {
          setState(() {
            _progress = 0.5 + (nativeProg * 0.5);
            _statusMessage = 'C++ Обработка: ${(nativeProg * 100).toInt()}%';
          });
        },
      );

      if (success) {
        setState(() {
          _progress = 1.0;
          _statusMessage = 'Успешно завършено!\nФайловете са запазени в:\n${outDir.path}';
        });
      } else {
        throw Exception('C++ ядрото върна грешка при обработката!');
      }

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
                  const SizedBox(height: 12),
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
                    padding: const EdgeInsets.all(10),
                    decoration: BoxDecoration(
                      color: const Color(0xFF282836),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Text(
                      _statusMessage,
                      style: const TextStyle(fontSize: 13, color: Colors.white70),
                      textAlign: TextAlign.center,
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
