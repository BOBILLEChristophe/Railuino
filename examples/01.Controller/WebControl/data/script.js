let isPowerOn = false;
const locos = {};
let selectedLoco = null;

// Classe Loco
class Loco {
    constructor(address) {
        this.address = address;
        this.speed = 0;
        this.direction = 0;
        this.functions = Array(10).fill(false);
    }

    setSpeed(speed) {
        this.speed = speed;
    }

    setDirection(direction) {
        this.direction = direction;
    }

    setFunction(index, state) {
        this.functions[index] = state;
    }
}

// Créer des instances de Loco pour chaque bouton image
document.querySelectorAll('.address-button').forEach(button => {
    const address = button.getAttribute('data-address');
    locos[address] = new Loco(address);
});

// Fonction pour mettre à jour l'affichage avec les valeurs de la locomotive sélectionnée
function updateUI() {
    if (selectedLoco) {
        document.getElementById('speedSlider').value = selectedLoco.speed;
        updateSpeedValue(selectedLoco.speed);
        document.querySelectorAll('.function-button').forEach((button, index) => {
            if (selectedLoco.functions[index]) {
                button.classList.add('active');
            } else {
                button.classList.remove('active');
            }
        });
    }
}

// Fonction pour ajouter un message au log
// function addLogMessage(message) {
//     const logWindow = document.getElementById('logWindow');
//     const newMessage = document.createElement('div');
//     newMessage.textContent = message;
//     logWindow.appendChild(newMessage);
//     logWindow.scrollTop = logWindow.scrollHeight;
// }

function addLogMessage(message) {
    const logWindow = document.getElementById('logWindow');
    logWindow.textContent = message;  // Remplace le contenu du log par le nouveau message
}

// Gestionnaire d'événement pour le bouton d'alimentation
document.getElementById('powerButton').addEventListener('click', () => {
    fetch('/setPower', { method: 'POST' })
        .then(response => response.text())
        .then(state => {
            isPowerOn = state === 'true';
            const powerButton = document.getElementById('powerButton');
            if (isPowerOn == true) {
                powerButton.classList.remove('power-off');
                powerButton.classList.add('power-on');
                addLogMessage('Power ON');
            } else if (isPowerOn == false) {
                powerButton.classList.remove('power-on');
                powerButton.classList.add('power-off');
                addLogMessage('Power OFF');
            } else {
                addLogMessage('Error for power function');
            }
        });
});

document.getElementById('systemHaltButton').addEventListener('click', () => {
    if (selectedLoco) {
        fetch(`/setSystemHalt?address=0x0000`, { method: 'POST' })
            .then(() => {
                document.getElementById('speedSlider').value = 0;
                updateSpeedValue(0);
                selectedLoco.setSpeed(0);
                addLogMessage(`System Halt`);
            });
    }
});

document.getElementById('stopButton').addEventListener('click', () => {
    if (selectedLoco) {
        fetch(`/setStop?address=${selectedLoco.address}`, { method: 'POST' })
            .then(() => {
                document.getElementById('speedSlider').value = 0;
                updateSpeedValue(0);
                selectedLoco.setSpeed(0);
                addLogMessage(`Stop for address ${selectedLoco.address}`);
            });
    }
});

document.getElementById('directionButton').addEventListener('click', () => {
    if (selectedLoco) {
        selectedLoco.direction = 1 - selectedLoco.direction; // Change de direction
        fetch(`/setDirection?address=${selectedLoco.address}`, { method: 'POST' })
            .then(() => {
                document.getElementById('speedSlider').value = 0;
                updateSpeedValue(0);
                addLogMessage(`Direction change for address ${selectedLoco.address}`);
            });
    }
});

document.getElementById('speedSlider').addEventListener('input', (event) => {
    if (!isPowerOn) {
        addLogMessage('Power is off');
        event.target.value = 0; // Réinitialiser le curseur à 0
        updateSpeedValue(0);
        return;
    }
    if (!selectedLoco) {
        addLogMessage('Select a locomotive');
        event.target.value = 0; // Réinitialiser le curseur à 0
        updateSpeedValue(0);
        return;
    }
    const speed = event.target.value;
    fetch(`/setSpeed?address=${selectedLoco.address}&speed=${speed}`, { method: 'POST' });
    selectedLoco.setSpeed(speed);
    updateSpeedValue(speed);
    addLogMessage(`Speed set to ${speed} for address ${selectedLoco.address}`);
});

document.querySelectorAll('.address-button').forEach(button => {
    button.addEventListener('click', () => {
        // Supprimer la classe 'selected' de tous les boutons
        document.querySelectorAll('.address-button').forEach(btn => btn.classList.remove('selected'));
        // Ajouter la classe 'selected' au bouton cliqué
        button.classList.add('selected');
        // Mettre à jour la locomotive sélectionnée
        const address = button.getAttribute('data-address');
        selectedLoco = locos[address];
        // Envoyer l'adresse au serveur
        fetch(`/setAddress?address=${address}`, { method: 'POST' });
        // Mettre à jour l'interface utilisateur avec les valeurs de la locomotive sélectionnée
        updateUI();
        addLogMessage(`Locomotive selected with address ${address}`);
    });
});

document.querySelectorAll('.function-button').forEach(button => {
    button.addEventListener('click', () => {
        if (selectedLoco) {
            const functionId = button.getAttribute('data-function');
            const newState = !selectedLoco.functions[functionId];
            fetch(`/setFunction?address=${selectedLoco.address}&function=${functionId}&power=${newState ? 1 : 0}`, { method: 'POST' })
                .then(() => {
                    selectedLoco.setFunction(functionId, newState);
                    if (newState) {
                        button.classList.add('active');
                        addLogMessage(`Function F ${functionId} activated for address ${selectedLoco.address}`);
                    } else {
                        button.classList.remove('active');
                        addLogMessage(`Function F ${functionId} deactivated for address ${selectedLoco.address}`);
                    }
                });
        }
    });
});

function updateSpeedValue(speed) {
    const speedValue = document.getElementById('speedValue');
    const percentage = (speed / 1000) * 100;
    speedValue.textContent = Math.round(percentage);
}
