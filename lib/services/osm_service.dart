import 'dart:io';
import 'package:http/http.dart' as http;
import 'package:path_provider/path_provider.dart';

class OsmService {
  static const List<String> overpassEndpoints = [
    'https://overpass-api.de/api/interpreter',
    'https://overpass.kumi.systems/api/interpreter',
    'https://maps.mail.ru/osm/tools/overpass/api/interpreter',
  ];

  static Future<File> downloadOsmArea({
    required double minLat,
    required double minLon,
    required double maxLat,
    required double maxLon,
    required Function(double progress, String status) onProgress,
  }) async {
    onProgress(0.1, 'Подготовка на Overpass заявка...');

    final query = '[out:xml][timeout:90];'
        '('
        'way["building"]($minLat,$minLon,$maxLat,$maxLon);'
        'relation["building"]($minLat,$minLon,$maxLat,$maxLon);'
        'way["highway"]($minLat,$minLon,$maxLat,$maxLon);'
        'way["natural"]($minLat,$minLon,$maxLat,$maxLon);'
        'way["leisure"]($minLat,$minLon,$maxLat,$maxLon);'
        'way["landuse"]($minLat,$minLon,$maxLat,$maxLon);'
        ');'
        '(._;>;);'
        'out body;';

    onProgress(0.2, 'Сваляне на данни от OpenStreetMap...');

    final tempDir = await getTemporaryDirectory();
    final filePath = '${tempDir.path}/map_data_${DateTime.now().millisecondsSinceEpoch}.osm';
    final file = File(filePath);

    Exception? lastError;

    for (final endpoint in overpassEndpoints) {
      try {
        final uri = Uri.parse(endpoint);
        final request = http.Request('POST', uri);
        request.headers.addAll({
          'User-Agent': 'GtaVcBuilderApp/1.0 (https://github.com/stanbg96/gtavc_builder)',
          'Accept': '*/*',
          'Content-Type': 'application/x-www-form-urlencoded; charset=UTF-8',
        });
        request.bodyFields = {'data': query};

        final response = await request.send();

        if (response.statusCode == 200) {
          final sink = file.openWrite();
          int downloadedBytes = 0;
          final contentLength = response.contentLength ?? -1;

          await response.stream.listen((chunk) {
            downloadedBytes += chunk.length;
            sink.add(chunk);
            if (contentLength > 0) {
              final progress = 0.2 + (downloadedBytes / contentLength) * 0.2;
              onProgress(progress, 'Свалени ${(downloadedBytes / 1024).toStringAsFixed(1)} KB');
            } else {
              onProgress(0.30, 'Свалени ${(downloadedBytes / 1024).toStringAsFixed(1)} KB');
            }
          }).asFuture();

          await sink.flush();
          await sink.close();

          onProgress(0.4, 'OSM данните са свалени успешно!');
          return file;
        } else {
          lastError = Exception('Сървърът $endpoint върна HTTP ${response.statusCode}');
        }
      } catch (e) {
        lastError = Exception('Грешка при връзка с $endpoint: $e');
      }
    }

    throw lastError ?? Exception('Неуспешно изтегляне на OSM данни');
  }

  // Сваляне на сателитна снимка за AI анализ
  static Future<File?> downloadSatelliteTile({
    required double minLat,
    required double minLon,
    required double maxLat,
    required double maxLon,
    required Function(double progress, String status) onProgress,
  }) async {
    try {
      onProgress(0.42, 'Сваляне на сателитна снимка за AI анализ...');
      final url = 'https://services.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/export'
          '?bbox=$minLon,$minLat,$maxLon,$maxLat&bboxSR=4326&size=512,512&imageSR=4326&format=png&f=image';

      final response = await http.get(Uri.parse(url), headers: {
        'User-Agent': 'GtaVcBuilderApp/1.0',
      });

      if (response.statusCode == 200 && response.bodyBytes.isNotEmpty) {
        final tempDir = await getTemporaryDirectory();
        final file = File('${tempDir.path}/sat_${DateTime.now().millisecondsSinceEpoch}.png');
        await file.writeAsBytes(response.bodyBytes);
        onProgress(0.48, 'Сателитната снимка е готова за AI обработка!');
        return file;
      }
    } catch (_) {}
    return null;
  }
}
