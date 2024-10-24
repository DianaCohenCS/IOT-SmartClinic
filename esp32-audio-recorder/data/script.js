let led = false;

async function toggleLED() {
    led = !led;
    try { // wait for response, don't forget to use ".local" with the name
        const response = await fetch("http://demo-server.local/led", {
            method: "PUT",
            body: JSON.stringify({ on: led }), // on parameter, with the value of led
            headers: {
                "Content-Type": "application/json",
            },
        });
    } catch (error) {
        alert("Request failed - check the console");
        console.error(error);
    }
}