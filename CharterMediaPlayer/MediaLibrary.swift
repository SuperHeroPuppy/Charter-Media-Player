import AVFoundation
import Combine
import Foundation

struct MediaEntry: Codable, Identifiable {
    let id: UUID
    let title: String
    let storedName: String
}

final class MediaLibrary: ObservableObject {
    @Published private(set) var items: [MediaEntry] = []
    @Published private(set) var currentID: UUID?
    @Published private(set) var isPlaying = false
    @Published var errorMessage: String?

    let player = AVPlayer()
    private let mediaDirectory: URL
    private let catalogURL: URL

    var currentItem: MediaEntry? { items.first { $0.id == currentID } }

    init() {
        let support = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask).first!
        mediaDirectory = support.appendingPathComponent("CharterMedia", isDirectory: true)
        catalogURL = support.appendingPathComponent("CharterLibrary.json")
        if let data = try? Data(contentsOf: catalogURL),
           let saved = try? JSONDecoder().decode([MediaEntry].self, from: data) {
            items = saved
        }
    }

    func importFiles(_ sources: [URL]) {
        let destination = mediaDirectory
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            var additions: [MediaEntry] = []
            var firstError: String?
            do {
                try FileManager.default.createDirectory(at: destination,
                                                        withIntermediateDirectories: true)
                for source in sources {
                    let scoped = source.startAccessingSecurityScopedResource()
                    defer { if scoped { source.stopAccessingSecurityScopedResource() } }
                    let storedName = UUID().uuidString + "-" + source.lastPathComponent
                    let target = destination.appendingPathComponent(storedName)
                    do {
                        try FileManager.default.copyItem(at: source, to: target)
                        additions.append(MediaEntry(id: UUID(), title: source.lastPathComponent,
                                                    storedName: storedName))
                    } catch {
                        firstError = error.localizedDescription
                    }
                }
            } catch {
                firstError = error.localizedDescription
            }
            DispatchQueue.main.async {
                guard let self else { return }
                self.items.append(contentsOf: additions)
                self.save()
                self.errorMessage = firstError
            }
        }
    }

    private func save() {
        do {
            let data = try JSONEncoder().encode(items)
            try data.write(to: catalogURL, options: .atomic)
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    func play(_ entry: MediaEntry) {
        let url = mediaDirectory.appendingPathComponent(entry.storedName)
        guard FileManager.default.fileExists(atPath: url.path) else {
            errorMessage = "The imported file is missing."
            return
        }
        try? AVAudioSession.sharedInstance().setCategory(.playback)
        player.replaceCurrentItem(with: AVPlayerItem(url: url))
        player.play()
        currentID = entry.id
        isPlaying = true
    }

    func togglePlayback() {
        if currentID == nil {
            if let first = items.first { play(first) }
        } else if isPlaying {
            player.pause()
            isPlaying = false
        } else {
            player.play()
            isPlaying = true
        }
    }

    func skip(_ offset: Int) {
        guard !items.isEmpty else { return }
        let index = items.firstIndex { $0.id == currentID } ?? 0
        let next = (index + offset + items.count) % items.count
        play(items[next])
    }
}
