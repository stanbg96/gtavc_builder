import 'package:flutter/material.dart';
import 'screens/map_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const GtaVcBuilderApp());
}

class GtaVcBuilderApp extends StatelessWidget {
  const GtaVcBuilderApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'GTA VC OSM Builder',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        primaryColor: const Color(0xFFFF007F),
        scaffoldBackgroundColor: const Color(0xFF121218),
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFFFF007F),
          secondary: Color(0xFF00F0FF),
          surface: Color(0xFF1E1E28),
        ),
        useMaterial3: true,
      ),
      home: const MapScreen(),
    );
  }
}
