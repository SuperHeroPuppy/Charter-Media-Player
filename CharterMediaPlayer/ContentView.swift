import AVKit
import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @StateObject private var library = MediaLibrary()
    @State private var showImporter = false

    var body: some View {
        NavigationStack {
            VStack(spacing: 12) {
                if library.currentItem != nil {
                    VideoPlayer(player: library.player)
                        .frame(height: 220)
                } else {
                    VStack(spacing: 12) {
                        Image(systemName: "music.note").font(.largeTitle)
                        Text("Nothing playing").font(.headline)
                        Text("Import media to get started").font(.subheadline)
                    }
                    .frame(maxWidth: .infinity)
                    .frame(height: 220)
                }
                Text(library.currentItem?.title ?? "Library ready")
                    .font(.headline)
                    .lineLimit(1)
                    .padding(.horizontal)
                HStack(spacing: 28) {
                    Button { library.skip(-1) } label: {
                        Image(systemName: "backward.end.fill")
                    }
                    Button { library.togglePlayback() } label: {
                        Image(systemName: library.isPlaying ? "pause.fill" : "play.fill")
                    }
                    Button { library.skip(1) } label: {
                        Image(systemName: "forward.end.fill")
                    }
                }
                .font(.title2)
                .buttonStyle(.bordered)
                List(library.items) { item in
                    Button {
                        library.play(item)
                    } label: {
                        HStack {
                            Image(systemName: "music.note")
                            Text(item.title).lineLimit(1)
                            Spacer()
                            if item.id == library.currentID {
                                Image(systemName: "waveform")
                            }
                        }
                    }
                }
            }
            .navigationTitle("Charter")
            .toolbar {
                Button { showImporter = true } label: {
                    Label("Import media", systemImage: "plus")
                }
            }
            .fileImporter(isPresented: $showImporter, allowedContentTypes: [.audio, .movie],
                          allowsMultipleSelection: true) { result in
                switch result {
                case .success(let urls): library.importFiles(urls)
                case .failure(let error): library.errorMessage = error.localizedDescription
                }
            }
            .alert("Import failed", isPresented: Binding(
                get: { library.errorMessage != nil },
                set: { if !$0 { library.errorMessage = nil } }
            )) {
                Button("OK") { library.errorMessage = nil }
            } message: {
                Text(library.errorMessage ?? "")
            }
            .tint(.orange)
        }
    }
}
