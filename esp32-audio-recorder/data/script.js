const uploadButton = document.getElementById('submit-form');
const recordButton = document.getElementById('record-button');
const playButton = document.getElementById('play-button');
const deleteButton = document.getElementById('delete-button');
// const statusDiv = document.getElementById('status');

let filepath = ""; // for play or delete
let filename_record = ""; // for recording only
let record_time = 0; // default value for recording duration

let isUploading = false;
let isRecording = false;
let isPlaying = false;
let isDeleting = false;
let isBusy = false;
let isUploadAvailable = false
let isRecordAvailable = false;
let isPlayAvailable = false;
let isDeleteAvailable = false;

function setUploading(state) {
    isUploading = state;
    updateButtonState();
}
function setRecording(state) {
    isRecording = state;
    updateButtonState();
}
function setPlaying(state) {
    isPlaying = state;
    updateButtonState();
}
function setDeleting(state) {
    isDeleting = state;
    updateButtonState();
}
function setUploadAvailable() {
    isUploadAvailable = true;
    updateButtonState();
}
function setRecordAvailable() {
    isRecordAvailable = true;
    updateButtonState();
}
function setPlayAvailable() {
    isPlayAvailable = true;
    updateButtonState();
}
function setDeleteAvailable() {
    isDeleteAvailable = true;
    updateButtonState();
}

function updateButtonState() {
    isBusy = isUploading || isRecording || isPlaying || isDeleting;

    uploadButton.value = isUploading ? 'Uploading...' : 'Upload';
    uploadButton.disabled = isBusy || !isUploadAvailable;

    recordButton.textContent = isRecording ? 'Recording...' : 'Start Recording';
    recordButton.disabled = isBusy || !isRecordAvailable;

    playButton.textContent = isPlaying ? 'Playing...' : 'Play on ESP';
    playButton.disabled = isBusy || !isPlayAvailable;

    deleteButton.textContent = isDeleting ? 'Deleting...' : 'Delete from FS';
    deleteButton.disabled = isBusy || !isDeleteAvailable;
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function getPlaylist() {
    fetch('/playlist')
        .then(response => response.json())
        .then(data => {
            const table = document.getElementById('list-table');
            const dropdown = document.getElementById('list-select');
            document.getElementById('play-container').style.display = (data.length > 0) ? "grid" : "none";

            data.forEach(audio_file => {
                row = table.insertRow();
                row.insertCell(0).textContent = audio_file.name;
                row.insertCell(1).textContent = audio_file.size;

                let option = document.createElement("option");
                option.setAttribute('value', audio_file.path);
                let optionText = document.createTextNode(audio_file.name);
                option.appendChild(optionText);

                dropdown.appendChild(option);
            });
        });
}

function getSpace() {
    fetch('/space')
        .then(response => response.json())
        .then(data => {
            const space = document.getElementById('fs-space');
            space.textContent = 'Available FS space: ' + data.space;
        });
}

async function uploadFiles(formData) {
    if (!isUploading) {
        try {
            setUploading(true);
            // perform a single call for all the selected files together
            const res = await fetch('/upload', {
                method: 'post',
                body: formData
            });
            console.log('[upload response status]', res.status);
            const data = await res.text(); // don't forget to wait for data
            console.log('[response data]', data);
        }
        catch (error) {
            console.error('[upload request failed]', error.message);
        }
        finally {
            location.replace("/"); // goto home with no 'go back' option
        }
    }
}

async function deleteAudio() {
    if (!isDeleting) {
        try {
            setDeleting(true);

            var formData = new FormData();
            formData.append("file", filepath);
            const res = await fetch('/edit', {
                method: 'delete',
                body: formData
            });
            console.log('[delete response status]', res.status);
            const data = await res.text(); // don't forget to wait for data
            console.log('[response data]', data);
        }
        catch (error) {
            console.error('[delete request failed]', error.message);
        }
        finally {
            location.replace("/"); // goto home with no 'go back' option
        }
    }
}

async function playAudio() {
    console.log('Playing: wait until ready...');
    await waitForReady(); // wait until ready
    if (!isPlaying) {
        try {
            setPlaying(true);
            
            var formData = new FormData();
            formData.append("file", filepath);
            console.log('[play request]');
            const res = await fetch('/play', {
                method: 'post',
                body: formData
            });
            console.log('[play response status]', res.status);
            const data = await res.text(); // don't forget to wait for data
            console.log('[response data]', data);

            await waitForReady(); // Start checking playback status
            console.log('[play back from status]');
        }
        catch (error) {
            console.error('[play request failed]', error.message);
        }
        finally {
            setPlaying(false);
            //location.replace("/"); // goto home with no 'go back' option
        }
    }
}

async function startRecording() {
    console.log('Recording: wait until ready...');
    await waitForReady(); // wait until ready
    if (!isRecording) {
        try {
            setRecording(true);

            console.log('[record_time]', record_time);
            var formData = new FormData();
            formData.append("record_time", record_time);
            console.log('[record request]');
            const res = await fetch('/record', {
                method: 'post',
                body: formData
            });
            console.log('[record response status]', res.status);
            const data = await res.text(); // don't forget to wait for data
            console.log('[response data]', data);

            await waitForReady(); // Start checking playback status
            console.log('[record back from status]');
        }
        catch (error) {
            console.error('[record request failed]', error.message);
        }
        finally {
            //setRecording(false);
            location.replace("/"); // goto home with no 'go back' option
        }
    }
}

async function checkPlaybackStatus() {
    let ready = false;
    try {
        //console.log('[status request]', new Date());
        const res = await fetch('/status');
        //console.log('[response status]', res.status);
        const data = await res.text(); // don't forget to wait for data
        //console.log('[response data]', data);
        if (data == 'busy') {
            console.log('[status busy]', new Date());
        }
        else {
            console.log('[status ready]', new Date());
            ready = true;
        }
    }
    catch (error) {
        console.error('[status request failed]', error.message);
    }
    finally {
        return ready;
    }
}

async function waitForReady() {
    let ready = false;
    const maxTries = 60;
    let currTry = 0;
    while (!ready) {
        currTry++;
        if (currTry > maxTries) {
            console.log('[exit waiting due to max tries]', new Date());
            break;
        }
        ready = await checkPlaybackStatus();
        if (!ready)
            await sleep(10000); // wait for 10 seconds before next try
    }
    // ready now...
    setPlaying(false);
    setRecording(false);
}

// not needed anymore, resolved the problem on the server-side
/*
async function manualFetch() {
    //location.href = "/"; // goto home
    location.replace("/");
    location.reload(true); // refresh
    console.log('manual reload ok');
    try {
        console.log('fetching javascript');
        const response = await fetch("/script.js");
        console.log('fetching javascript ok');
        const data = await response.text();
    }
    catch (err) {
        console.error('[request failed]', err.message);
        console.log('fetching javascript failed');
    }
    finally {
        return true;
    }
}

// Handle upload of a single file
async function uploadFile(file, currTry = 1, retries = 3) {
    let formData = new FormData();
    formData.append('file', file);
    try {
        console.log('[attempt#]', currTry, '[out of retries]', retries);
        const res = await fetch('/upload', {
            method: 'post',
            body: formData
        });
        console.log('[status]', res.status);
        const data = await res.text();
        return res;
    }
    catch (error) {
        console.error('[request failed]', error.message);
        if (currTry < retries) {
            console.log('get another try');
            await manualFetch();
            return await uploadFile(file, ++currTry);
        }
        else {
            return error;
        }
    }
};
*/

function loadBody() {
    // disable the page while loading
    updateButtonState();

    // populate the elements with the playlist
    getSpace();
    getPlaylist();

    // now enable the upload button
    setUploadAvailable();

    // not needed anymore, resolved the problem on the server-side
    /*
    // Handle form submission, can have multiple files for upload
    // document.getElementById('upload-form').addEventListener('submit', async function (e) {
    //     const inputFiles = document.getElementById('uploads').files; // get the FileList object
    //     let numOfFiles = inputFiles.length;
    //     console.log('You selected', numOfFiles, 'files');

    //     //const delay = (ms = 1000) => new Promise(r => setTimeout(r, ms));
    //     for (let i = 0; i < numOfFiles; i++) {
    //         let currFile = inputFiles.item(i);
    //         console.log('Trying to upload', currFile.name);
    //         // ensure the file is of accepted type
    //         if (!currFile.name.endsWith(".wav") && !currFile.name.endsWith(".mp3")) {
    //             console.log('Unsupported file extension');
    //             continue;
    //         }
    //         // File path can be 31 characters maximum in SPIFFS, including the "/" prefix
    //         if (currFile.name.length > 30) {
    //             console.log('File name is too long');
    //             continue;
    //         }
    //         let res = await uploadFile(currFile);
    //         console.log('Returned from upload file with', res);
    //         //uploadFile(currFile);
    //         //await delay();
    //     }
    //     alert('Files upload finished. Click OK to reload the page.');
    //     await manualFetch();
    //     // Refresh the page and bypass the cache
    //     //location.replace("/");
    //     //location.reload(true);
    // });
    */

    // Handle form submission
    document.getElementById('upload-form').addEventListener('submit', async function (e) {
        e.preventDefault();
        const formData = new FormData(this);
        await uploadFiles(formData); // wait to return...
    });

    // set the recording time in seconds according to the selection
    document.getElementById('record-select').addEventListener("change", e => {
        record_time = e.target.value;
        setRecordAvailable();
    });

    // set the filename to play according to the selection
    document.getElementById('list-select').addEventListener("change", e => {
        filepath = e.target.value;
        document.getElementById('play-browser').src = filepath;
        setPlayAvailable();
        setDeleteAvailable();
    });
}

// this is our main
loadBody();
