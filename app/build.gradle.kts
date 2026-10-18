plugins {
    id("com.android.application")
    kotlin("android")
}

val fbneoDir = project.findProperty("fbneoDir") as String?
    ?: System.getenv("FBNEO_DIR")
    ?: "${projectDir}/src/main/cpp/fbneo"

android {
    namespace = "com.fbneo.android"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.fbneo.android"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
        ndk { abiFilters += listOf("arm64-v8a") }
        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                arguments += "-DFBNEO_DIR=$fbneoDir"
                arguments += "-DANDROID_STL=c++_static"
            }
        }
    }

    externalNativeBuild {
        cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" }
    }

    packaging { jniLibs { useLegacyPackaging = true } }
}
