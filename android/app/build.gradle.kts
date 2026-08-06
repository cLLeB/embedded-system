import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// Signing material, kept out of the repository. Both this file and the .jks it
// points at are gitignored: between them they can publish an update that
// Android accepts as genuine, so they are backed up separately rather than
// committed. Without the file the project still builds - release is simply
// unsigned, which is the right behaviour for anyone who clones this.
val keystorePropertiesFile = rootProject.file("keystore.properties")
val keystoreProperties = Properties().apply {
    if (keystorePropertiesFile.exists()) {
        keystorePropertiesFile.inputStream().use { load(it) }
    }
}

android {
    namespace = "gh.group38.smartsocket"
    compileSdk = 35

    defaultConfig {
        applicationId = "gh.group38.smartsocket"

        // 26 rather than lower: the foreground service that keeps the link alive
        // in the background is an O API, and there is no point supporting a
        // platform the app's main feature cannot run on.
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
    }

    signingConfigs {
        if (keystorePropertiesFile.exists()) {
            create("release") {
                storeFile = rootProject.file(keystoreProperties.getProperty("storeFile"))
                storePassword = keystoreProperties.getProperty("storePassword")
                keyAlias = keystoreProperties.getProperty("keyAlias")
                keyPassword = keystoreProperties.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")

            // Null when keystore.properties is absent, which leaves an unsigned
            // APK rather than failing the build. An unsigned APK that will not
            // install is a clearer signal than a build error about a missing
            // file somebody was never given.
            signingConfig = signingConfigs.findByName("release")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2024.09.03")
    implementation(composeBom)

    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.6")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.6")
    implementation("androidx.activity:activity-compose:1.9.2")

    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")

    // Plain JVM tests, no Robolectric and no emulator. Everything worth testing
    // here - the wire format, the reconnect schedule, the export - was written
    // to have no Android in it for exactly that reason, the same split that lets
    // the firmware's src/core run on a PC.
    testImplementation("junit:junit:4.13.2")
}
