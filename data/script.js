document.getElementById('powerButton').addEventListener('click', () => {
    fetch('/setPower', { method: 'POST' })
        .then(response => response.text())
        .then(state => {
            const powerButton = document.getElementById('powerButton');
            if (state === 'true') {
                powerButton.classList.remove('power-off');
                powerButton.classList.add('power-on');
            } else {
                powerButton.classList.remove('power-on');
                powerButton.classList.add('power-off');
            }
        });
});

document.getElementById('stopButton').addEventListener('click', () => {
    fetch('/setStop', { method: 'POST' }).then(() => {
        document.getElementById('speedSlider').value = 0;
    });
});

document.getElementById('emergencyButton').addEventListener('click', () => {
    fetch('/setEmergency', { method: 'POST' }).then(() => {
        document.getElementById('speedSlider').value = 0;
    });
});

document.getElementById('directionButton').addEventListener('click', () => {
    fetch('/setDirection', { method: 'POST' }).then(() => {
        document.getElementById('speedSlider').value = 0;
    });
});

document.getElementById('speedSlider').addEventListener('input', (event) => {
    fetch(`/setSpeed?speed=${event.target.value}`, { method: 'POST' });
});

document.querySelectorAll('.address-button').forEach(button => {
    button.addEventListener('click', () => {
        const address = button.getAttribute('data-address');
        fetch(`/setAddress?address=${address}`, { method: 'POST' });
    });
});
