package fr.ycdev.poseidon;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter; // BLUETOOTH: Import
import android.bluetooth.BluetoothDevice;  // BLUETOOTH: Import
import android.bluetooth.BluetoothSocket;  // BLUETOOTH: Import
import android.content.BroadcastReceiver;   // BLUETOOTH: Import
import android.content.Context;
import android.content.Intent;            // BLUETOOTH: Import
import android.content.IntentFilter;     // BLUETOOTH: Import
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.location.Location;
import android.location.LocationManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;              // BLUETOOTH: Import
import android.os.Looper;             // BLUETOOTH: Import
import android.os.Message;            // BLUETOOTH: Import
import android.util.Log;                // BLUETOOTH: Import pour le débogage
import android.view.View;               // BLUETOOTH: Import
import android.widget.Button;            // BLUETOOTH: Import
import android.widget.TextView;
import android.widget.Toast;            // BLUETOOTH: Import

import androidx.annotation.NonNull;    // BLUETOOTH: Import (peut être nécessaire pour le Handler)
import androidx.core.app.ActivityCompat; // BLUETOOTH: Import
import androidx.core.content.ContextCompat;

import org.osmdroid.api.IMapController;
import org.osmdroid.config.Configuration;
import org.osmdroid.tileprovider.tilesource.TileSourceFactory;
import org.osmdroid.util.GeoPoint;
import org.osmdroid.views.MapView;
import org.osmdroid.views.overlay.Marker;

import java.io.IOException;            // BLUETOOTH: Import
import java.io.InputStream;// BLUETOOTH: Import
import java.io.OutputStream;           // BLUETOOTH: Import
import java.util.ArrayList;
import java.util.Locale;
import java.util.Set;                  // BLUETOOTH: Import
import java.util.UUID;                 // BLUETOOTH: Import

import org.json.JSONException;
import org.json.JSONObject;

import org.osmdroid.events.MapEventsReceiver;
import org.osmdroid.views.overlay.MapEventsOverlay;

import org.osmdroid.views.overlay.Polyline;

public class MainActivity extends Activity {

    private TextView tvText_l;
    private TextView tvText_r;

    private MapView mvOsmMap;
    private GeoPoint myLocationPoint;
    private Marker currentLocationMarker;
    private RepeatingUiTask locationUpdater;

    // deux drawables pour les marqueurs rouge et vert
    private Drawable redDotDrawable;
    private Drawable greenDotDrawable;
    private Drawable blueSquareDrawable;
    private Marker waypointMarker;
    // Variables pour la ligne et les infos de navigation
    private org.osmdroid.views.overlay.Polyline polylineToWaypoint;
    private TextView tvNavInfo;
    private TextView tvModeInfo;
    private TextView tvBatteryInfo;

    // BLUETOOTH: Variables membres Bluetooth
    private static final String TAG = "MainActivityBluetooth";
    private static final int REQUEST_ENABLE_BT = 101;
    private static final int REQUEST_BLUETOOTH_PERMISSIONS = 102; // Pour les permissions d'exécution BT
    private static final String TARGET_DEVICE_NAME = "Poseidon"; // nom de du module BT
    private static final UUID MY_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB"); // UUID Standard pour SPP

    private Button btnConnectBluetooth;
    private Button btnCenterMap;
    private TextView tvBluetoothStatus; // Optionnel, si vous voulez un TextView dédié à l'état BT

    private BluetoothAdapter bluetoothAdapter;
    private BluetoothDevice targetDevice;
    private BluetoothSocket bluetoothSocket;
    private ConnectedThread connectedThread;
    private Handler dataHandler; // Pour recevoir les données du ConnectedThread

    // dernière localisation valide (du tel ou de l'ESP)
    private Location lastValidLocation;


    // BLUETOOTH: BroadcastReceiver pour la découverte d'appareils (si nécessaire)

    // BLUETOOTH_PARSE: Variables pour stocker les dernières données reçues
    private volatile double dr_temp_bt = Double.NaN;
    private volatile double dr_turb_bt = Double.NaN;
    private volatile double dr_ph_bt = Double.NaN;
    private volatile double dr_res_bt = Double.NaN;

    private final BroadcastReceiver discoveryReceiver = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (BluetoothDevice.ACTION_FOUND.equals(action)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (ActivityCompat.checkSelfPermission(MainActivity.this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                    // Les permissions devraient déjà être gérées, mais c'est une sécurité
                    return;
                }
                if (device != null && device.getName() != null && device.getName().equals(TARGET_DEVICE_NAME)) {
                    targetDevice = device;
                    Log.d(TAG, "Appareil cible trouvé: " + device.getName() + " [" + device.getAddress() + "]");
                    Toast.makeText(MainActivity.this, "Appareil " + TARGET_DEVICE_NAME + " trouvé.", Toast.LENGTH_SHORT).show();
                    bluetoothAdapter.cancelDiscovery(); // Arrêter la découverte une fois trouvé
                    connectToDevice(targetDevice);
                }
            } else if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(action)) {
                Log.d(TAG, "Découverte terminée.");
                if (targetDevice == null) {
                    Toast.makeText(MainActivity.this, "Appareil " + TARGET_DEVICE_NAME + " non trouvé.", Toast.LENGTH_LONG).show();
                    if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion");
                }
            }
        }
    };

    private class PoseidonMapEventsReceiver implements MapEventsReceiver {

        @Override
        public boolean singleTapConfirmedHelper(GeoPoint p) {
            // Non utilisé pour le moment, mais requis par l'interface
            return false;
        }

        @Override
        public boolean longPressHelper(GeoPoint p) {
            // C'est ici que la magie opère !
            Log.d(TAG, "Appui long détecté à: " + p.getLatitude() + ", " + p.getLongitude());

            // Met à jour la position du marqueur de waypoint
            waypointMarker.setPosition(p);

            // Si c'est le premier appui, on rend le marqueur visible
            // FIX: Use isEnabled() instead of isVisible()
            if (!waypointMarker.isEnabled()) {
                // FIX: Use setEnabled(true) instead of setVisible(true)
                waypointMarker.setEnabled(true);
            }

            // Redessine la carte pour afficher le marqueur à sa nouvelle position
            mvOsmMap.postInvalidate();

            // Au relâchement du doigt (implicite à la fin de l'appui long),
            // on envoie les coordonnées.
            sendWaypointCoordinates(p);
            updateNavigationInfo();

            // On retourne 'true' pour indiquer qu'on a bien traité l'événement
            return true;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initGUI();

        // BLUETOOTH: Initialisation du Handler pour les données reçues
        dataHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(@NonNull Message msg) {
                if (msg.what == ConnectedThread.MESSAGE_READ) {
                    try {
                        byte[] readBuf = (byte[]) msg.obj;
                        String readMessage = new String(readBuf, 0, msg.arg1).trim();
                        // On passe simplement le message au parser, il s'occupe de tout.
                        parseBluetoothData(readMessage);
                    } catch (Exception e) {
                        Log.e(TAG, "Erreur de traitement du message Handler", e);
                    }
                }
            }
        };


        // BLUETOOTH: Initialisation Bluetooth
        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (bluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth non supporté sur cet appareil.", Toast.LENGTH_LONG).show();
            // Gérer le cas où BT n'est pas supporté (par ex. désactiver le bouton BT)
            if (btnConnectBluetooth != null) btnConnectBluetooth.setEnabled(false);
            return;
        }

        // BLUETOOTH: Configuration de l'action du bouton
        if (btnConnectBluetooth != null) {
            btnConnectBluetooth.setOnClickListener(v -> {
                if (connectedThread != null && connectedThread.isAlive()) {
                    // Déjà connecté, donc déconnecter
                    disconnectBluetooth();
                } else {
                    // Pas connecté, essayer de se connecter
                    checkBluetoothPermissionsAndConnect();
                }
            });
        }
    }

    private void initGUI() {
        // 1. Définir le layout UNE SEULE FOIS
        setContentView(R.layout.layout_main);

        // 2. Récupérer tous les éléments UI
        tvText_l = findViewById(R.id.tv_text_l);
        tvText_r = findViewById(R.id.tv_text_r);
        mvOsmMap = findViewById(R.id.mv_osmmap);
        btnConnectBluetooth = findViewById(R.id.btn_connect_bluetooth);
        btnCenterMap = findViewById(R.id.btn_center_map);
        //tvBluetoothStatus = findViewById(R.id.tv_bluetooth_status);
        tvNavInfo = findViewById(R.id.tv_nav_info);
        tvModeInfo = findViewById(R.id.tv_mode_info);
        tvBatteryInfo = findViewById(R.id.tv_battery_info);

        // 3. Charger les icônes (drawables)
        redDotDrawable = ContextCompat.getDrawable(this, R.drawable.red_dot);
        greenDotDrawable = ContextCompat.getDrawable(this, R.drawable.green_dot);
        blueSquareDrawable = ContextCompat.getDrawable(this, R.drawable.blue_square);

        // 4. Configurer le marqueur de position actuelle
        currentLocationMarker = new Marker(mvOsmMap);
        currentLocationMarker.setIcon(redDotDrawable); // Rouge par défaut
        // Le currentLocationMarker sera ajouté à la carte dynamiquement dans onLocationChanged

        // 5. Configurer le marqueur de waypoint
        waypointMarker = new Marker(mvOsmMap);
        waypointMarker.setIcon(blueSquareDrawable);
        waypointMarker.setEnabled(false); // Invisible au démarrage
        mvOsmMap.getOverlayManager().add(waypointMarker); // Ajouté une seule fois à la carte

        // Configurer la Polyline
        polylineToWaypoint = new org.osmdroid.views.overlay.Polyline(mvOsmMap);
        polylineToWaypoint.getPaint().setStyle(android.graphics.Paint.Style.STROKE);
        polylineToWaypoint.getPaint().setPathEffect(new android.graphics.DashPathEffect(new float[]{20, 10}, 0)); // Dashed line
        polylineToWaypoint.getPaint().setColor(android.graphics.Color.BLUE); // Use Android's Color class
        polylineToWaypoint.getPaint().setStrokeWidth(5);
        polylineToWaypoint.setEnabled(false); // In osmdroid, use setEnabled(false) to hide it
        mvOsmMap.getOverlayManager().add(polylineToWaypoint);


        // 6. Configurer le texte des boutons et statuts
        if (tvBluetoothStatus != null) tvBluetoothStatus.setText("BT: Déconnecté");
        if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion");

        // 7. Configurer la carte (vous l'aviez oublié dans la version finale)
        Configuration.getInstance().load(getApplicationContext(), getPreferences(MODE_PRIVATE));
        mvOsmMap.setTileSource(TileSourceFactory.MAPNIK);
        mvOsmMap.setMultiTouchControls(true);
        IMapController mapController = mvOsmMap.getController();
        mapController.setZoom(15.0);

        // 8. Activer la détection des appuis longs sur la carte
        MapEventsOverlay mapEventsOverlay = new MapEventsOverlay(new PoseidonMapEventsReceiver());
        mvOsmMap.getOverlays().add(0, mapEventsOverlay);
        // activer le bouton de repositionnement de la carte
        btnCenterMap.setOnClickListener(v -> {
            if (lastValidLocation != null) {
                GeoPoint currentPos = new GeoPoint(lastValidLocation);
                mvOsmMap.getController().animateTo(currentPos);
            } else {
                Toast.makeText(MainActivity.this, "Position actuelle non disponible", Toast.LENGTH_SHORT).show();
            }
        });

    }


    @Override
    protected void onResume() {
        super.onResume();
        // La demande de permission de localisation est déjà gérée ici
        requestPermissions(); // Cela gère déjà les permissions de localisation

        // BLUETOOTH: Enregistrer le BroadcastReceiver pour la découverte
        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_FOUND);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);
        registerReceiver(discoveryReceiver, filter);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (locationUpdater != null) {
            locationUpdater.stop();
        }
        // BLUETOOTH: Désenregistrer le BroadcastReceiver
        unregisterReceiver(discoveryReceiver);
        // BLUETOOTH: Optionnel: déconnecter le Bluetooth pour économiser la batterie
        // disconnectBluetooth(); // Décommentez si vous voulez déconnecter en pause
    }

    @Override
    protected void onDestroy() { // BLUETOOTH: Ajouter onDestroy
        super.onDestroy();
        disconnectBluetooth(); // S'assurer que tout est nettoyé
    }

    private void requestPermissions() {
        // Cette méthode gère déjà ACCESS_FINE_LOCATION et ACCESS_COARSE_LOCATION
        // Nous allons la laisser telle quelle pour la localisation.
        // Les permissions Bluetooth seront demandées séparément avant de tenter une opération BT.

        String[] locationPermissions = {
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
        };

        boolean allLocationPermissionsGranted = true;
        for (String permission : locationPermissions) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                allLocationPermissionsGranted = false;
                break;
            }
        }

        if (allLocationPermissionsGranted) {
            onPermissionsGranted(); // Pour la localisation
        } else {
            ActivityCompat.requestPermissions(this, locationPermissions, 1); // Code 1 pour la localisation
        }
    }


    // BLUETOOTH: Nouvelle méthode pour vérifier les permissions BT et lancer la connexion
    private void checkBluetoothPermissionsAndConnect() {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.BLUETOOTH_CONNECT}, REQUEST_BLUETOOTH_PERMISSIONS);
                return; // Attendre le résultat de la demande de permission
            }
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
            return;
        }

        // Vérifier les permissions pour Android 12+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED ||
                    ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT},
                        REQUEST_BLUETOOTH_PERMISSIONS);
                return; // Attendre le résultat
            }
        } else { // Pour Android < 12, BLUETOOTH et BLUETOOTH_ADMIN suffisent (déjà dans le manifest)
            // et ACCESS_FINE_LOCATION pour la découverte
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "Permission de localisation requise pour la découverte BT.", Toast.LENGTH_LONG).show();
                // La permission de localisation est demandée par requestPermissions()
                // On peut supposer qu'elle sera accordée ou déjà accordée.
            }
        }
        // Si les permissions sont OK, chercher les appareils
        findBluetoothDevice();
    }


    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults); // Appel à super important
        if (requestCode == 1) { // Permissions de Localisation
            boolean allGranted = true;
            for (int grantResult : grantResults) {
                if (grantResult != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false;
                    break;
                }
            }
            if (allGranted) {
                onPermissionsGranted(); // Pour la localisation
            } else {
                Toast.makeText(this, "Permissions de localisation refusées.", Toast.LENGTH_SHORT).show();
                // finish(); // Ne pas finir l'appli si les permissions BT peuvent encore être demandées
            }
        } else if (requestCode == REQUEST_BLUETOOTH_PERMISSIONS) { // BLUETOOTH: Gérer les permissions BT
            boolean allBtPermissionsGranted = true;
            for (int grantResult : grantResults) {
                if (grantResult != PackageManager.PERMISSION_GRANTED) {
                    allBtPermissionsGranted = false;
                    break;
                }
            }
            if (allBtPermissionsGranted) {
                Toast.makeText(this, "Permissions Bluetooth accordées.", Toast.LENGTH_SHORT).show();
                // Essayer à nouveau de se connecter/scanner après avoir obtenu les permissions
                findBluetoothDevice();
            } else {
                Toast.makeText(this, "Permissions Bluetooth refusées.", Toast.LENGTH_LONG).show();
                if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion");
            }
        }
    }
    // Méthode pour envoyer les coordonnées du waypoint via Bluetooth
    private void sendWaypointCoordinates(GeoPoint p) {
        // On n'envoie que si on est connecté
        if (connectedThread == null || !connectedThread.isAlive()) {
            Toast.makeText(this, "Non connecté, le waypoint ne peut pas être envoyé.", Toast.LENGTH_SHORT).show();
            return;
        }

        try {
            // Création de l'objet JSON
            JSONObject waypointJson = new JSONObject();
            waypointJson.put("lat", p.getLatitude());
            waypointJson.put("lng", p.getLongitude());

            // Formatage final de la chaîne à envoyer
            String messageToSend = "WP:" + waypointJson.toString() + "\n"; // Le \n est souvent utile comme délimiteur de fin de message

            // Envoi des données via le ConnectedThread
            connectedThread.write(messageToSend.getBytes());

            Log.d(TAG, "Waypoint envoyé: " + messageToSend.trim());
            Toast.makeText(this, "Waypoint envoyé à " + String.format(Locale.US, "%.5f", p.getLatitude()), Toast.LENGTH_SHORT).show();

        } catch (JSONException e) {
            Log.e(TAG, "Erreur lors de la création du JSON pour le waypoint", e);
        }
    }


    @SuppressLint("MissingPermission") // Les permissions sont vérifiées avant cet appel
    private void onPermissionsGranted() { // Concerne principalement la localisation maintenant
        if (tvText_l != null) { // Vérifier si tvText n'est pas null
            // tvText.setText(R.string.text_permissions_granted); // On peut garder ou modifier ce message
        }

        Configuration.getInstance().setUserAgentValue(getPackageName());
        mvOsmMap.setTileSource(TileSourceFactory.MAPNIK);
        mvOsmMap.setMultiTouchControls(true);
        MapEventsOverlay mapEventsOverlay = new MapEventsOverlay(new PoseidonMapEventsReceiver());
        mvOsmMap.getOverlays().add(0, mapEventsOverlay); // On le met en premier pour qu'il reçoive bien les clics
        IMapController controller = mvOsmMap.getController();
        controller.setZoom(18.0);

        LocationManager locationManager = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
        if (locationManager != null) {
            try {
                locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000, 0.5f, loc -> {});
                locationUpdater = new RepeatingUiTask(() -> {
                    // Si on n'est pas (ou plus) connecté en Bluetooth...
                    if (connectedThread == null || !connectedThread.isAlive()) {
                        // ...on réinitialise les valeurs des capteurs à "non disponible".
                        dr_temp_bt = Double.NaN;
                        dr_turb_bt = Double.NaN;
                        dr_ph_bt = Double.NaN;
                        dr_res_bt = Double.NaN;

                        // ...et on utilise le GPS du téléphone pour mettre à jour l'affichage.
                        Location location = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER);
                        onLocationChanged(location); // Cette méthode affichera "--" pour les capteurs.
                    }
                    // Si on est connecté, on ne fait absolument RIEN ici.
                    // Le Handler s'occupera de tout via les messages de l'ESP32.
                }, 500); // Exécuté toutes les 500ms
                locationUpdater.start();
            } catch (SecurityException e) {
                Log.e(TAG, "Erreur de permission de localisation dans onPermissionsGranted", e);
                Toast.makeText(this, "Erreur de permission de localisation.", Toast.LENGTH_SHORT).show();
            }
        }
    }

    // BLUETOOTH: Logique de recherche et connexion
    @SuppressLint("MissingPermission") // Permissions vérifiées dans checkBluetoothPermissionsAndConnect
    private void findBluetoothDevice() {
        if (bluetoothAdapter == null ) return;
        if (!bluetoothAdapter.isEnabled()){
            Toast.makeText(this, "Bluetooth non activé.", Toast.LENGTH_SHORT).show();
            if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion");
            return;
        }

        // D'abord, vérifier les appareils déjà appairés
        Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();
        if (pairedDevices != null && !pairedDevices.isEmpty()) {
            for (BluetoothDevice device : pairedDevices) {
                if (device.getName() != null && device.getName().equals(TARGET_DEVICE_NAME)) {
                    targetDevice = device;
                    Log.d(TAG, "Appareil cible trouvé dans les appairés: " + device.getName());
                    Toast.makeText(this, "Appareil " + TARGET_DEVICE_NAME + " appairé trouvé.", Toast.LENGTH_SHORT).show();
                    connectToDevice(targetDevice);
                    return;
                }
            }
        }

        // Si non trouvé dans les appairés, lancer la découverte
        if (bluetoothAdapter.isDiscovering()) {
            bluetoothAdapter.cancelDiscovery();
        }
        Log.d(TAG, "Lancement de la découverte Bluetooth...");
        Toast.makeText(this, "Recherche de " + TARGET_DEVICE_NAME + "...", Toast.LENGTH_SHORT).show();
        if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Recherche...");
        boolean discoveryStarted = bluetoothAdapter.startDiscovery();
        if(!discoveryStarted){
            Log.e(TAG, "Erreur au démarrage de la découverte");
            Toast.makeText(this, "Erreur découverte BT.", Toast.LENGTH_SHORT).show();
            if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion");
        }
    }

    @SuppressLint("MissingPermission") // Permissions vérifiées
    private void connectToDevice(BluetoothDevice device) {
        if (device == null || bluetoothAdapter == null) return;
        if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion...");

        new Thread(() -> {
            try {
                if (bluetoothAdapter.isDiscovering()) {
                    bluetoothAdapter.cancelDiscovery();
                }
                bluetoothSocket = device.createRfcommSocketToServiceRecord(MY_UUID);
                bluetoothSocket.connect(); // Appel bloquant

                // Connexion réussie
                runOnUiThread(() -> {
                    Toast.makeText(MainActivity.this, "Connecté à " + device.getName(), Toast.LENGTH_SHORT).show();
                    if (tvBluetoothStatus != null) tvBluetoothStatus.setText("BT: Connecté à " + device.getName());
                    if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Déconnecter");
                    if (tvModeInfo != null) tvModeInfo.setVisibility(View.VISIBLE);
                    if (tvBatteryInfo != null) tvBatteryInfo.setVisibility(View.VISIBLE);
                    // Changer la couleur du marqueur en vert
                    if (currentLocationMarker != null) {
                        currentLocationMarker.setIcon(greenDotDrawable);
                    }
                });
                    // Forcer un rafraîchissement initial de l'affichage juste après la connexion
                    // pour ne pas attendre le premier message GPS de l'ESP32.
                    runOnUiThread(() -> {
                        // On met les capteurs en état "attente"
                        dr_temp_bt = Double.NaN;
                        dr_turb_bt = Double.NaN;
                        dr_ph_bt = Double.NaN;
                        dr_res_bt = Double.NaN;

                        // On utilise la dernière position connue (même si c'est celle du téléphone)
                        // comme position de départ. Elle sera très vite remplacée par celle de l'ESP32.
                        // Si lastValidLocation est null, on passe null, onLocationChanged gérera.
                        onLocationChanged(lastValidLocation);
                });

                connectedThread = new ConnectedThread(bluetoothSocket);
                connectedThread.start();


            } catch (IOException e) {
                Log.e(TAG, "Erreur de connexion Bluetooth: " + e.getMessage());
                try {
                    if (bluetoothSocket != null) bluetoothSocket.close();
                } catch (IOException closeException) {
                    Log.e(TAG, "Impossible de fermer le socket lors de l'erreur de connexion", closeException);
                }
                runOnUiThread(() -> {
                    Toast.makeText(MainActivity.this, "Erreur connexion: " + e.getMessage(), Toast.LENGTH_SHORT).show();
                    if (tvBluetoothStatus != null) tvBluetoothStatus.setText("BT: Erreur connexion");
                    if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion");
                });
            }
        }).start();
    }


    private void disconnectBluetooth() {
        if (connectedThread != null) {
            connectedThread.cancel();
            connectedThread = null;
        }
        if (bluetoothSocket != null) {
            try {
                bluetoothSocket.close();
            } catch (IOException e) {
                Log.e(TAG, "Erreur lors de la fermeture du socket Bluetooth", e);
            }
            bluetoothSocket = null;
        }
        if (tvBluetoothStatus != null) tvBluetoothStatus.setText("BT: Déconnecté");
        if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion");
        Toast.makeText(this, "Déconnecté du Bluetooth", Toast.LENGTH_SHORT).show();
        if (tvModeInfo != null) tvModeInfo.setVisibility(View.GONE);
        if (tvBatteryInfo != null) tvBatteryInfo.setVisibility(View.GONE);
        Log.d(TAG, "Bluetooth déconnecté.");
        // Change la couleur du marqueur en rouge
        if (currentLocationMarker != null) {
            currentLocationMarker.setIcon(redDotDrawable);
        }
    }
    // Méthode pour mettre à jour la ligne, la distance et l'azimut
    private void updateNavigationInfo() {
        // On n'affiche les infos que si :
        // 1. On est connecté
        // 2. Le marqueur du waypoint est activé (visible)
        // 3. On a une position courante valide
        boolean isConnected = (connectedThread != null && connectedThread.isAlive());
        boolean isWaypointSet = (waypointMarker != null && waypointMarker.isEnabled());
        boolean hasCurrentLocation = (lastValidLocation != null);

        if (isConnected && isWaypointSet && hasCurrentLocation) {
            // Obtenir les GeoPoints
            GeoPoint currentPos = new GeoPoint(lastValidLocation);
            GeoPoint waypointPos = waypointMarker.getPosition();

            // 1. Mettre à jour la ligne pointillée
            ArrayList<GeoPoint> pathPoints = new ArrayList<>();
            pathPoints.add(currentPos);
            pathPoints.add(waypointPos);
            polylineToWaypoint.setPoints(pathPoints);
            polylineToWaypoint.setVisible(true);

            // 2. Calculer la distance et l'azimut
            float[] results = new float[3];
            Location.distanceBetween(
                    currentPos.getLatitude(), currentPos.getLongitude(),
                    waypointPos.getLatitude(), waypointPos.getLongitude(),
                    results);

            float distanceInMeters = results[0];
            float bearingInDegrees = results[1]; // Azimut initial
            if (bearingInDegrees < 0) {
                bearingInDegrees += 360; // Assurer une valeur positive
            }

            // 3. Mettre à jour le TextView
            String navText = String.format(Locale.FRANCE, "Distance: %.0f m | Direction: %.0f°",
                    distanceInMeters, bearingInDegrees);
            tvNavInfo.setText(navText);
            tvNavInfo.setVisibility(View.VISIBLE);

        } else {
            // Cacher la ligne et les infos si les conditions ne sont pas remplies
            polylineToWaypoint.setVisible(false);
            if (tvNavInfo != null) {
                tvNavInfo.setVisibility(View.GONE);
            }
            // Si on n'est pas connecté, on cache aussi le marqueur du waypoint
            if (!isConnected && waypointMarker != null) {
                waypointMarker.setEnabled(false);
            }
        }

        // Redessiner la carte pour que les changements soient visibles
        mvOsmMap.postInvalidate();
    }


    // BLUETOOTH: Classe interne pour la gestion de la connexion
    private class ConnectedThread extends Thread {
        private final BluetoothSocket mmSocket;
        private final InputStream mmInStream;
        private final OutputStream mmOutStream;
        private byte[] mmBuffer; // mmBuffer store for the stream

        public static final int MESSAGE_READ = 0;
        public static final int MESSAGE_WRITE = 1;
        public static final int MESSAGE_TOAST = 2;


        public ConnectedThread(BluetoothSocket socket) {
            mmSocket = socket;
            InputStream tmpIn = null;
            OutputStream tmpOut = null;

            try {
                tmpIn = socket.getInputStream();
            } catch (IOException e) {
                Log.e(TAG, "Erreur lors de la création du InputStream", e);
            }
            try {
                tmpOut = socket.getOutputStream();
            } catch (IOException e) {
                Log.e(TAG, "Erreur lors de la création du OutputStream", e);
            }
            mmInStream = tmpIn;
            mmOutStream = tmpOut;
        }

        public void run() {
            mmBuffer = new byte[1024];
            int numBytes; // bytes returned from read()

            while (true) {
                try {
                    numBytes = mmInStream.read(mmBuffer);
                    // Envoyer les bytes obtenus à l'UI Activity via le Handler
                    if (dataHandler != null) {
                        dataHandler.obtainMessage(MESSAGE_READ, numBytes, -1, mmBuffer).sendToTarget();
                    }
                } catch (IOException e) {
                    Log.d(TAG, "InputStream déconnecté", e);
                    // Gérer la déconnexion depuis ce thread
                    // Par exemple, en informant l'activité principale
                    runOnUiThread(() -> {
                        Toast.makeText(MainActivity.this, "Connexion BT perdue.", Toast.LENGTH_LONG).show();
                        disconnectBluetooth(); // Appeler la méthode de déconnexion principale
                    });
                    break;
                }
            }
        }

        public void write(byte[] bytes) {
            try {
                mmOutStream.write(bytes);
                // Partager le message envoyé avec l'UI Activity si nécessaire via Handler
                // dataHandler.obtainMessage(MESSAGE_WRITE, -1, -1, bytes).sendToTarget();
            } catch (IOException e) {
                Log.e(TAG, "Erreur lors de l'envoi de données", e);
                // Gérer l'erreur d'écriture, par exemple en informant l'activité principale
                // Message msg = handler.obtainMessage(MESSAGE_TOAST);
                // Bundle bundle = new Bundle();
                // bundle.putString("toast", "Couldn't send data");
                // msg.setData(bundle);
                // handler.sendMessage(msg);
            }
        }

        public void cancel() {
            try {
                mmSocket.close();
            } catch (IOException e) {
                Log.e(TAG, "Impossible de fermer le socket de connexion", e);
            }
        }
    }


    @Override // Modifié pour potentiellement inclure les données BT
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_ENABLE_BT) {
            if (resultCode == Activity.RESULT_OK) {
                Toast.makeText(this, "Bluetooth activé.", Toast.LENGTH_SHORT).show();
                // Essayer de trouver l'appareil maintenant que BT est activé
                findBluetoothDevice();
            } else {
                Toast.makeText(this, "Activation Bluetooth annulée.", Toast.LENGTH_SHORT).show();
                if (btnConnectBluetooth != null) btnConnectBluetooth.setText("Connexion");
            }
        }
    }

    // Cette méthode est appelée par le locationUpdater
    private void onLocationChanged(Location location) {
        // Si la localisation est nulle, on affiche un message et on sort.
        if (location == null) {
            tvText_l.setText("En attente de données GPS...");
            return;
        }

        // Met à jour la dernière localisation valide
        this.lastValidLocation = location;

        // ... récupération de latitude, longitude, altitude, vitesse ...
        int satellites = 0;
        if (location.getExtras() != null) {
            satellites = location.getExtras().getInt("satellites");
        }
        float accuracy = location.getAccuracy();
        double latitude = location.getLatitude();
        double longitude = location.getLongitude();
        double altitude = location.getAltitude();
        double speed = location.getSpeed() * 3.6; // km/h


        String s = "";
        s += "Nb satellites: " + satellites + "\n";
        s += "Latitude: " + String.format(Locale.US, "%.6f", latitude) + "\n";
        s += "Longitude: " + String.format(Locale.US, "%.6f", longitude) + "\n";
        s += "Précision: " + String.format(Locale.US, "%.1f", accuracy) + " m\n";
        s += "Altitude: " + String.format(Locale.US, "%.2f", altitude) + " m\n";
        s += "Vitesse: " + String.format(Locale.US, "%.2f", speed) + " km/h\n\n";
        if (tvText_l != null) tvText_l.setText(s);
        // Affichage conditionnel des données des capteurs
        // Si la valeur est NaN, on affiche "--", sinon on affiche la valeur formatée.

        s = "";
        // Affichage conditionnel des données des capteurs
        s += "temperature: " + (Double.isNaN(dr_temp_bt) ? "--" : String.format(Locale.US, "%.1f", dr_temp_bt)) + " °c\n";
        s += "turbidite: " + (Double.isNaN(dr_turb_bt) ? "--" : String.format(Locale.US, "%.1f", dr_turb_bt)) + " NTU\n";
        s += "acidite: " + (Double.isNaN(dr_ph_bt) ? "--" : String.format(Locale.US, "%.1f", dr_ph_bt)) + "\n";
        s += "resistance: " + (Double.isNaN(dr_res_bt) ? "--" : String.format(Locale.US, "%.0f", dr_res_bt)) + " Ohms\n"; // Utilisez %.0f si c'est un entier

        if (tvText_r != null) tvText_r.setText(s);

        // ... (Mise à jour de la carte MapView comme avant) ...
        IMapController controller = mvOsmMap.getController();
        if (myLocationPoint == null) {
            myLocationPoint = new GeoPoint(latitude, longitude);
            controller.setCenter(myLocationPoint);
        } else {
            myLocationPoint.setLatitude(latitude);
            myLocationPoint.setLongitude(longitude);
        }
        mvOsmMap.getOverlayManager().remove(currentLocationMarker);
        currentLocationMarker.setPosition(myLocationPoint);
        mvOsmMap.getOverlayManager().add(currentLocationMarker);
        mvOsmMap.postInvalidate();
        updateNavigationInfo();
    }



    // BLUETOOTH_PARSE: Méthode pour parser la chaîne de données reçue de l'ESP32
    // BLUETOOTH_PARSE: Méthode pour parser la chaîne de données reçue de l'ESP32
    private void parseBluetoothData(String data) {
        Log.d(TAG, "Données reçues pour parsing: " + data);

        try {
            if (data.startsWith("SEN:")) {
                // C'est un message de capteurs
                // NOUVEAU FORMAT: SEN:{"temp":25.5,"turb":50,"res":240,"ph":7.2,"bat":100,"mode":"M"}
                String jsonString = data.substring(4);
                JSONObject sensorsJson = new JSONObject(jsonString);

                // Les données sont maintenant à la racine de l'objet JSON.
                if (sensorsJson.has("temp")) {
                    dr_temp_bt = sensorsJson.getDouble("temp");
                }
                if (sensorsJson.has("turb")) {
                    dr_turb_bt = sensorsJson.getDouble("turb");
                }
                if (sensorsJson.has("ph")) {
                    dr_ph_bt = sensorsJson.getDouble("ph");
                }
                if (sensorsJson.has("res")) { // Prise en compte de la nouvelle valeur "res"
                    dr_res_bt = sensorsJson.getDouble("res");
                }
                // extrait bat et mode si nécessaire pour l'affichage
                int valeurBatterie = sensorsJson.optInt("bat", -1); // -1 si non trouvé
                String nouveauMode = sensorsJson.optString("mode", "--"); // "--" si non trouvé
                // Mettre à jour l'interface sur le thread UI
                runOnUiThread(() -> {
                    if (tvBatteryInfo != null) {
                        if (valeurBatterie != -1) {
                            tvBatteryInfo.setText("Bat: " + valeurBatterie + "%");
                        } else {
                            tvBatteryInfo.setText("Bat: --%");
                        }
                    }
                    if (tvModeInfo != null) {
                        tvModeInfo.setText("Mode: " + nouveauMode);
                    }
                });

                Log.d(TAG, "Parsing SEN: Temp=" + dr_temp_bt + ", Turb=" + dr_turb_bt + ", pH=" + dr_ph_bt + ", Res=" + dr_res_bt);

                // Après avoir mis à jour les valeurs des capteurs, on rafraîchit l'affichage
                // pour qu'elles apparaissent avec la dernière position GPS connue.
                updateDisplayedData();

            } else if (data.startsWith("GPS:")) {
                // C'est un message GPS de l'ESP32
                // NOUVEAU FORMAT: GPS:{"lat":47.14...,"lng":-1.67...,"alt":63.9,...,"ts":321521}
                String jsonString = data.substring(4);
                JSONObject gpsJson = new JSONObject(jsonString);

                // Construire un objet Location à partir des données de l'ESP32
                Location espLocation = new Location("esp32_gps_provider");

                // Les données sont maintenant à la racine de l'objet JSON.
                espLocation.setLatitude(gpsJson.getDouble("lat"));
                espLocation.setLongitude(gpsJson.getDouble("lng"));

                if (gpsJson.has("alt")) {
                    espLocation.setAltitude(gpsJson.getDouble("alt"));
                }
                if (gpsJson.has("hdop")) {
                    // L'accuracy est en mètres. Le HDOP n'est pas directement l'accuracy,
                    // mais on peut l'utiliser pour avoir une idée de la précision.
                    // Ici, on le définit comme l'accuracy pour l'affichage.
                    espLocation.setAccuracy((float) gpsJson.getDouble("hdop"));
                }

                // Stocker le nombre de satellites dans les "extras"
                Bundle extras = new Bundle();
                if (gpsJson.has("sat")) {
                    extras.putInt("satellites", gpsJson.getInt("sat"));
                }
                espLocation.setExtras(extras);

                // Le timestamp de la localisation est important
                espLocation.setTime(System.currentTimeMillis());

                Log.d(TAG, "Parsing GPS de l'ESP32: Lat=" + espLocation.getLatitude() + ", Lng=" + espLocation.getLongitude() + ", Sat=" + gpsJson.optInt("sat", 0));

                // Appeler onLocationChanged avec les données de l'ESP32
                // L'appel doit être sur le thread UI car il modifie les vues.
                runOnUiThread(() -> onLocationChanged(espLocation));
            }
        } catch (JSONException e) {
            Log.e(TAG, "Erreur de parsing JSON pour la donnée: " + data, e);
        } catch (Exception e) {
            Log.e(TAG, "Erreur inattendue dans parseBluetoothData", e);
        }
    }

    // Méthode pour mettre à jour l'affichage
    private void updateDisplayedData() {
        // MODIFICATION: Ne plus chercher le GPS du téléphone ici.
        // On rappelle simplement onLocationChanged avec la dernière localisation
        // qu'on a en mémoire, pour rafraîchir l'affichage avec les nouvelles
        // données de capteurs.
        if (this.lastValidLocation != null) {
            onLocationChanged(this.lastValidLocation);
        }
    }
/*
    @Override
    public void onBackPressed() {
        myLocationPoint = null;
        super.onBackPressed();
    } */
}
