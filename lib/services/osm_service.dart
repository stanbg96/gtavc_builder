import 'dart:io';
import 'package:http/http.dart' as http;
import 'package:path_provider/path_provider.dart';

class OsmService {
  static const String overpassUrl = 'https://overpass-api.de/api/interpreter';

  static Future<File> downloadOsmArea({
    required double minLat,
    required double minLon,
    required double maxLat,
    required double maxLon,
    required Function(double progress, String status) onProgress,
  }) async {
    onProgress(0.1, 'Подготовка на Overpass заявка...');

    final query = '''
[out:xml][timeout:90];
(
  way["building"]($minLat,$minLon,$maxLat,$maxLon);
  relation["building"]($minLat,$minLon,$maxLat,$maxLon);
  way["highway"]($minLat,$minLon,$maxLat,$maxLon);
);
(._;>;);
out body;
''';

    onProgress(0.2, 'Сваляне на OpenStreetMap данни от сървъра...');

    final uri = Uri.parse(overpassUrl);
    final client = http.Client();
    final request = http.Request('POST', uri)
      ..bodyFields = {'data': query};

    final response = await client.send(request);

    if (response.statusCode != 200) {
      throw Exception('Грешка при сваляне на OSM: HTTP ${response.statusCode}');
    }

    final tempDir = await getTemporaryDirectory();
    final filePath = '${tempDir.path}/map_data_${DateTime.now().millisecondsSinceEpoch}.osm';
    final file = File(filePath);
    final sink = file.openWrite();

    int downloadedBytes = 0;
    final contentLength = response.contentLength ?? -1;

    await response.stream.listen((chunk) {
      sink.add(chunk);
      downloadedBytes += chunk.length;
      if (contentLength > 0) {
        final progress = 0.2 + (downloadedBytes / contentLength) * 0.3;
        onProgress(progress, 'Свалени ${(downloadedBytes / 1024).toStringAsFixed(1)} KB');
      } else {
        onProgress(0.35, 'Свалени ${(downloadedBytes / 1024).toStringAsFixed(1)} KB');
      }
    }).asFuture();

    await sink.flush();
    await sink.close();

    onProgress(0.5, 'OSM данните са свалени успешно!');
    return file;
  }
}
