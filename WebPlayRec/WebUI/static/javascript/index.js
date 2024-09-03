// Modified from https://github.com/mdn/webaudio-examples/tree/main/audio-basics for teaching purposes

console.clear();

// instigate our audio context
let audioCtx;

// load some sound
const audioElement = document.querySelector("audio");
let track;

const playButton = document.querySelector(".tape-controls-play");

// play pause audio
playButton.addEventListener(
    "click",
    () => {
        if (!audioCtx) {
            init();
        }

        // check if context is in suspended state (autoplay policy)
        if (audioCtx.state === "suspended") {
            audioCtx.resume();
        }

        function buttonPressed() {
            const myHeaders = new Headers();
            myHeaders.append("Content-Type", "application/json");
            var panValue = document.getElementById('panner').value;
            var volumeValue = document.getElementById('volume').value;

            console.log(JSON.stringify({ pan: panValue, volume: volumeValue }));
            fetch('/buttonPressed', {
                method: 'POST', headers: myHeaders,
                body: JSON.stringify({ pan: panValue, volume: volumeValue })

            }).then((response) => response.json())
                .then((success) => {
                    console.log(success);
                });

        }

        if (playButton.dataset.playing === "false") {
            audioElement.play();
            playButton.dataset.playing = "true";
            buttonPressed();

            // if track is playing pause it
        } else if (playButton.dataset.playing === "true") {
            audioElement.pause();
            playButton.dataset.playing = "false";
            buttonPressed();
        }

        // Toggle the state between play and not playing
        let state =
            playButton.getAttribute("aria-checked") === "true" ? true : false;
        playButton.setAttribute("aria-checked", state ? "false" : "true");
    },
    false
);

// If track ends
audioElement.addEventListener(
    "ended",
    () => {
        playButton.dataset.playing = "false";
        playButton.setAttribute("aria-checked", "false");
    },
    false
);

function init() {
    audioCtx = new AudioContext();
    track = new MediaElementAudioSourceNode(audioCtx, {
        mediaElement: audioElement,
    });

    // Create the node that controls the volume.
    const gainNode = new GainNode(audioCtx);

    const volumeControl = document.querySelector('[data-action="volume"]');
    volumeControl.addEventListener(
        "input",
        () => {
            gainNode.gain.value = volumeControl.value;
        },
        false
    );

    // Create the node that controls the panning
    const panner = new StereoPannerNode(audioCtx, { pan: 0 });

    const pannerControl = document.querySelector('[data-action="panner"]');
    pannerControl.addEventListener(
        "input",
        () => {
            panner.pan.value = pannerControl.value;
        },
        false
    );

    // connect our graph
    track.connect(gainNode).connect(panner).connect(audioCtx.destination);
}
