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

  final TextEditingController _minLatController = TextEditingController(text: '42.01259');
  final TextEditingController _minLonController = TextEditingController(text: '23.09503');
  final TextEditingController _maxLatController = TextEditingController(text: '42.01408');
  final TextEditingController _maxLonController = TextEditingController(text: '23.09703');

  TargetPlatformType _selectedPlatform = TargetPlatformType.android;
  TexturingMode _selectedTexturingMode = TexturingMode.aiSatellite;
  bool _enableProps = true;
  int _selectedVehicleId = 141;
  bool _isProcessing = false;
  double _progress = 0.0;
  String _statusMessage = 'Изберете зона от картата и натиснете бутона долу.';
  String? _publicExportPath;

  final Map<int, String> _vehicles = {
    141: '🏎️ Ferrari Testarossa (Cheetah)',
    236: '🏎️ Lamborghini Countach (Infernus)',
    188: '🏎️ Porsche 911 Turbo (Comet)',
    205: '🏎️ BMW M3 E30 (Sentinel XS)',
    138: '🏎️ Mercedes-Benz 560 SEC (Admiral)',
    139: '🏎️ Ferrari Daytona (Stinger)',
    135: '🏎️ Mercedes 190E (Sentinel)',
    196: '🇯🇵 Honda CR-X (Blista Compact)',
    208: '🇯🇵 Toyota Supra Mk3 (Sabre Turbo)',
    189: '🇯🇵 Mazda RX-7 (Deluxo)',
    130: '🇯🇵 Toyota Land Cruiser (Landstalker)',
    191: '🏍️ Honda CBR / Suzuki (PCJ-600)',
    198: '🏍️ Yamaha Enduro (Sanchez)',
    168: '🛵 Honda Super Cub (Faggio)',
    155: '🚁 Hunter Chopper',
  };

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
            Text('Инструкции за GTA VC', style: TextStyle(fontSize: 16)),
          ],
        ),
        content: const SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('1. Копирай генерираната папка в:', style: TextStyle(fontWeight: FontWeight.bold, color: Colors.cyan, fontSize: 13)),
              Text('GTA Vice City/data/maps/osm/\n', style: TextStyle(fontFamily: 'monospace', fontSize: 11)),
              Text('2. Отвори data/gta_vc.dat и добави:', style: TextStyle(fontWeight: FontWeight.bold, color: Colors.cyan, fontSize: 13)),
              SelectableText(
                'CDIMAGE DATA\\MAPS\\OSM\\OSM_MAP.IMG\nIDE DATA\\MAPS\\OSM\\OSM_WORLD.IDE\nIPL DATA\\MAPS\\OSM\\OSM_WORLD.IPL\nCOLFILE 0 DATA\\MAPS\\OSM\\OSM_WORLD.COL',
                style: TextStyle(fontFamily: 'monospace', color: Colors.amber, fontSize: 10),
              ),
              SizedBox(height: 10),
              Text('3. Стартирай играта!', style: TextStyle(fontWeight: FontWeight.bold, color: Colors.greenAccent, fontSize: 13)),
              Text('Картата и колата ще те чакат на координати X:0 Y:0 Z:15.', style: TextStyle(fontSize: 12)),
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

      File? satFile;
      if (_selectedTexturingMode == TexturingMode.aiSatellite) {
        satFile = await OsmService.downloadSatelliteTile(
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
      }

      final appDir = await getApplicationDocumentsDirectory();
      final folderName = 'GTA_VC_MAP_${DateTime.now().millisecondsSinceEpoch}';
      final internalOutDir = Directory('${appDir.path}/$folderName');
      if (!await internalOutDir.exists()) {
        await internalOutDir.create(recursive: true);
      }

      setState(() {
        _statusMessage = _selectedTexturingMode == TexturingMode.aiSatellite
            ? 'AI Анализира сателитната визия и строи 3D сградите...'
            : 'C++ Енджинът генерира 32 архитектурни материала...';
      });

      final native = NativeBridge();
      final success = await native.convertOsmToGta(
        osmFilePath: osmFile.path,
        outputDirPath: internalOutDir.path,
        satImagePath: satFile?.path,
        platform: _selectedPlatform,
        enableProps: _enableProps,
        spawnVehicleId: _selectedVehicleId,
        texturingMode: _selectedTexturingMode,
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
        _statusMessage = ' Готово! Експортиран пълен пакет ($fileCount файла) в:\n${publicDownloadDir.path}';
      });

      if (await osmFile.exists()) await osmFile.delete();
      if (satFile != null && await satFile.exists()) await satFile.delete();
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
        title: const Text('GTA VC Map Builder', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
        backgroundColor: const Color(0xFF1E1E28),
        elevation: 0,
        actions: [
          IconButton(
            icon: const Icon(Icons.help_outline, color: Color(0xFF00F0FF)),
            tooltip: 'Инструкции за играта',
            onPressed: _showInstallGuideDialog,
          ),
          IconButton(
            icon: const Icon(Icons.crop_free, color: Color(0xFFFF007F)),
            tooltip: 'Вземи координати от екрана',
            onPressed: _isProcessing ? null : _updateBboxFromMap,
          ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(12, 8, 12, 16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Компактна Карта
            Container(
              height: 180,
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: Colors.white12),
              ),
              clipBehavior: Clip.antiAlias,
              child: Stack(
                children: [
                  FlutterMap(
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
                  Positioned(
                    right: 8,
                    bottom: 8,
                    child: FloatingActionButton.small(
                      backgroundColor: const Color(0xFF1E1E28),
                      foregroundColor: const Color(0xFF00F0FF),
                      onPressed: _updateBboxFromMap,
                      child: const Icon(Icons.my_location, size: 18),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 10),

            // Режим на текстуриране
            Row(
              children: [
                Expanded(
                  child: SegmentedButton<TexturingMode>(
                    segments: const [
                      ButtonSegment(value: TexturingMode.aiSatellite, label: Text('🛰️ AI Сателит', style: TextStyle(fontSize: 11))),
                      ButtonSegment(value: TexturingMode.procedural32, label: Text('🏛️ 32 Текстури', style: TextStyle(fontSize: 11))),
                    ],
                    selected: {_selectedTexturingMode},
                    onSelectionChanged: _isProcessing
                        ? null
                        : (newSelection) => setState(() => _selectedTexturingMode = newSelection.first),
                  ),
                ),
                const SizedBox(width: 8),
                SegmentedButton<TargetPlatformType>(
                  segments: const [
                    ButtonSegment(value: TargetPlatformType.pc, label: Text('PC', style: TextStyle(fontSize: 11))),
                    ButtonSegment(value: TargetPlatformType.android, label: Text('Android', style: TextStyle(fontSize: 11))),
                  ],
                  selected: {_selectedPlatform},
                  onSelectionChanged: _isProcessing
                      ? null
                      : (newSelection) => setState(() => _selectedPlatform = newSelection.first),
                ),
              ],
            ),
            const SizedBox(height: 8),

            // Избор на кола
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 2),
              decoration: BoxDecoration(
                color: const Color(0xFF1E1E28),
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.white24),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text('Стартова кола:', style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold)),
                  DropdownButtonHideUnderline(
                    child: DropdownButton<int>(
                      value: _selectedVehicleId,
                      dropdownColor: const Color(0xFF1E1E28),
                      items: _vehicles.entries.map((e) {
                        return DropdownMenuItem<int>(
                          value: e.key,
                          child: Text(e.value, style: const TextStyle(fontSize: 12, color: Color(0xFF00F0FF))),
                        );
                      }).toList>,
                      onChanged: _isProcessing ? null : (val) {
                        if (val != null) setState(() => _selectedVehicleId = val);
                      },
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 4),

            // Лампи и дървета
            SwitchListTile(
              dense: true,
              contentPadding: EdgeInsets.zero,
              title: const Text('Улични лампи и палми (Props)', style: TextStyle(fontSize: 12)),
              value: _enableProps,
              activeColor: const Color(0xFFFF007F),
              onChanged: _isProcessing ? null : (val) => setState(() => _enableProps = val),
            ),

            // Координати (Компактна решетка)
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _minLatController,
                    enabled: !_isProcessing,
                    decoration: const InputDecoration(labelText: 'Min Lat (Юг)', isDense: true, border: OutlineInputBorder()),
                    keyboardType: const TextInputType.numberWithOptions(decimal: true),
                    style: const TextStyle(fontSize: 12),
                  ),
                ),
                const SizedBox(width: 6),
                Expanded(
                  child: TextField(
                    controller: _minLonController,
                    enabled: !_isProcessing,
                    decoration: const InputDecoration(labelText: 'Min Lon (Запад)', isDense: true, border: OutlineInputBorder()),
                    keyboardType: const TextInputType.numberWithOptions(decimal: true),
                    style: const TextStyle(fontSize: 12),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 6),
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _maxLatController,
                    enabled: !_isProcessing,
                    decoration: const InputDecoration(labelText: 'Max Lat (Север)', isDense: true, border: OutlineInputBorder()),
                    keyboardType: const TextInputType.numberWithOptions(decimal: true),
                    style: const TextStyle(fontSize: 12),
                  ),
                ),
                const SizedBox(width: 6),
                Expanded(
                  child: TextField(
                    controller: _maxLonController,
                    enabled: !_isProcessing,
                    decoration: const InputDecoration(labelText: 'Max Lon (Изток)', isDense: true, border: OutlineInputBorder()),
                    keyboardType: const TextInputType.numberWithOptions(decimal: true),
                    style: const TextStyle(fontSize: 12),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 10),

            // Прогрес бар
            if (_isProcessing) ...[
              LinearProgressIndicator(
                value: _progress > 0 ? _progress : null,
                color: const Color(0xFFFF007F),
                backgroundColor: Colors.white12,
              ),
              const SizedBox(height: 6),
            ],

            // Статус и Бутони за копиране / инструкции
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: const Color(0xFF1E1E28),
                borderRadius: BorderRadius.circular(8),
                border: Border.all(
                  color: _publicExportPath != null ? const Color(0xFF00F0FF) : Colors.white12,
                ),
              ),
              child: Column(
                children: [
                  Text(
                    _statusMessage,
                    style: const TextStyle(fontSize: 12, color: Colors.white70),
                    textAlign: TextAlign.center,
                  ),
                  if (_publicExportPath != null) ...[
                    const SizedBox(height: 8),
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
                          icon: const Icon(Icons.copy, size: 14),
                          label: const Text('Копирай път', style: TextStyle(fontSize: 12)),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: const Color(0xFF00F0FF),
                            foregroundColor: Colors.black,
                            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                          ),
                        ),
                        const SizedBox(width: 8),
                        ElevatedButton.icon(
                          onPressed: _showInstallGuideDialog,
                          icon: const Icon(Icons.info_outline, size: 14),
                          label: const Text('Инструкции', style: TextStyle(fontSize: 12)),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: const Color(0xFFFF007F),
                            foregroundColor: Colors.white,
                            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                          ),
                        ),
                      ],
                    ),
                  ],
                ],
              ),
            ),
          ],
        ),
      ),
      // Закован постоянен бутон най-долу на екрана
      bottomNavigationBar: Container(
        padding: const EdgeInsets.all(12),
        decoration: const BoxDecoration(
          color: Color(0xFF1E1E28),
          border: Border(top: BorderSide(color: Colors.white12)),
        ),
        child: SafeArea(
          child: ElevatedButton.icon(
            onPressed: _isProcessing ? null : _startGeneration,
            icon: const Icon(Icons.build_circle),
            label: Text(
              _isProcessing ? 'ГЕНЕРИРАНЕ...' : 'ГЕНЕРИРАЙ GTA VC ФАЙЛОВЕ',
              style: const TextStyle(fontSize: 15, fontWeight: FontWeight.bold),
            ),
            style: ElevatedButton.styleFrom(
              backgroundColor: const Color(0xFFFF007F),
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(vertical: 14),
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
            ),
          ),
        ),
      ),
    );
  }
}
