let isPowerOn = false;
const locos = {};
let selectedLoco = null;

// Classe Loco
class Loco {
    constructor(address, uid) {
        this.address = address;
        this.uid = uid; // UID MFX (hexadécimal) ou null

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

    // Paramètres de requête : l'UID d'abord (le serveur en déduit l'adresse
    // actuelle), l'adresse en secours.
    params() {
        return (this.uid ? `uid=${this.uid}&` : '') + `address=${this.address}`;
    }

    // Nom affiché dans le journal
    label() {
        return this.uid ? `UID ${this.uid}` : `address ${this.address}`;
    }
}

// Clé d'une locomotive : son UID s'il est donné, sinon son adresse
function locoKey(button) {
    return button.getAttribute('data-uid') || button.getAttribute('data-address');
}

// Envoie une commande et affiche la réponse du serveur (elle contient
// l'adresse réellement utilisée).
function send(url, message) {
    return fetch(url, { method: 'POST' })
        .then(response => response.text().then(text => {
            addLogMessage(response.ok ? `${message} - ${text}` : `Error: ${text}`);
            return response.ok;
        }))
        .catch(() => {
            addLogMessage('No answer from the server');
            return false;
        });
}

// Créer des instances de Loco pour chaque bouton image
document.querySelectorAll('.address-button').forEach(button => {
    locos[locoKey(button)] = new Loco(button.getAttribute('data-address'), button.getAttribute('data-uid'));
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

// Bouton d'alimentation : son état vient de la centrale
function showPower(on) {
    isPowerOn = on;
    const powerButton = document.getElementById('powerButton');
    powerButton.classList.toggle('power-on', on);
    powerButton.classList.toggle('power-off', !on);
}

document.getElementById('powerButton').addEventListener('click', () => {
    fetch('/setPower', { method: 'POST' })
        .then(response => response.text().then(text => {
            if (!response.ok) {
                addLogMessage(`Error: ${text}`); // état inchangé
                return;
            }
            showPower(text === 'true');
            addLogMessage(isPowerOn ? 'Power ON' : 'Power OFF');
        }))
        .catch(() => addLogMessage('No answer from the server'));
});

// État réel (Stop / Go de la MS2 compris) au chargement, puis toutes les 3 s
function refreshPower() {
    fetch('/power')
        .then(response => response.text())
        .then(text => showPower(text === 'true'))
        .catch(() => {});
}
refreshPower();
setInterval(refreshPower, 3000);

document.getElementById('systemHaltButton').addEventListener('click', () => {
    if (selectedLoco) {
        cancelPendingSpeed();
        document.getElementById('speedSlider').value = 0;
        updateSpeedValue(0);
        selectedLoco.setSpeed(0);
        send(`/setSystemHalt`, 'System Halt');
    }
});

document.getElementById('stopButton').addEventListener('click', () => {
    if (selectedLoco) {
        cancelPendingSpeed();
        document.getElementById('speedSlider').value = 0;
        updateSpeedValue(0);
        selectedLoco.setSpeed(0);
        send(`/setStop?${selectedLoco.params()}`, `Stop for ${selectedLoco.label()}`);
    }
});

document.getElementById('directionButton').addEventListener('click', () => {
    if (selectedLoco) {
        selectedLoco.direction = 1 - selectedLoco.direction; // Change de direction
        cancelPendingSpeed();
        // La centrale arrête la loco au changement de sens
        document.getElementById('speedSlider').value = 0;
        updateSpeedValue(0);
        selectedLoco.setSpeed(0);
        send(`/setDirection?${selectedLoco.params()}`, `Direction change for ${selectedLoco.label()}`);
    }
});

// Curseur de vitesse : au plus une commande toutes les SPEED_INTERVAL_MS
// pendant le déplacement, et toujours la valeur finale au relâchement.
const SPEED_INTERVAL_MS = 100;
let lastSpeedSent = 0;   // instant du dernier envoi
let speedTimer = null;   // envoi différé en attente
let pendingSpeed = null; // { loco, speed } à envoyer

function sendSpeed() {
    speedTimer = null;
    if (!pendingSpeed)
        return;
    const { loco, speed } = pendingSpeed;
    pendingSpeed = null;
    lastSpeedSent = Date.now();
    send(`/setSpeed?${loco.params()}&speed=${speed}`, `Speed set to ${speed} for ${loco.label()}`);
}

function queueSpeed(speed, now) {
    pendingSpeed = { loco: selectedLoco, speed: speed };
    const wait = SPEED_INTERVAL_MS - (Date.now() - lastSpeedSent);
    if (now || wait <= 0) {
        if (speedTimer) {
            clearTimeout(speedTimer);
        }
        sendSpeed();
    } else if (!speedTimer) {
        speedTimer = setTimeout(sendSpeed, wait);
    }
}

function cancelPendingSpeed() {
    if (speedTimer) {
        clearTimeout(speedTimer);
        speedTimer = null;
    }
    pendingSpeed = null;
}

function speedAllowed(slider) {
    if (!isPowerOn) {
        addLogMessage('Power is off');
    } else if (!selectedLoco) {
        addLogMessage('Select a locomotive');
    } else {
        return true;
    }
    slider.value = 0; // Réinitialiser le curseur à 0
    updateSpeedValue(0);
    return false;
}

const speedSlider = document.getElementById('speedSlider');

speedSlider.addEventListener('input', (event) => {
    if (!speedAllowed(event.target))
        return;
    const speed = event.target.value;
    selectedLoco.setSpeed(speed);
    updateSpeedValue(speed);
    queueSpeed(speed, false);
});

// Relâchement du curseur (ou flèches du clavier) : valeur finale tout de suite
speedSlider.addEventListener('change', (event) => {
    if (!speedAllowed(event.target))
        return;
    queueSpeed(event.target.value, true);
});

document.querySelectorAll('.address-button').forEach(button => {
    button.addEventListener('click', () => {
        // Supprimer la classe 'selected' de tous les boutons
        document.querySelectorAll('.address-button').forEach(btn => btn.classList.remove('selected'));
        // Ajouter la classe 'selected' au bouton cliqué
        button.classList.add('selected');
        // Mettre à jour la locomotive sélectionnée
        cancelPendingSpeed(); // la vitesse en attente était pour l'autre loco
        selectedLoco = locos[locoKey(button)];
        // Mettre à jour l'interface utilisateur avec les valeurs de la locomotive sélectionnée
        updateUI();
        // Le serveur répond avec l'adresse qu'il utilisera
        send(`/setAddress?${selectedLoco.params()}`, `Locomotive selected (${selectedLoco.label()})`);
    });
});

document.querySelectorAll('.function-button').forEach(button => {
    button.addEventListener('click', () => {
        if (selectedLoco) {
            const functionId = button.getAttribute('data-function');
            const newState = !selectedLoco.functions[functionId];
            const loco = selectedLoco;
            const text = `Function F${functionId} ${newState ? 'activated' : 'deactivated'} for ${loco.label()}`;
            send(`/setFunction?${loco.params()}&function=${functionId}&power=${newState ? 1 : 0}`, text)
                .then(ok => {
                    if (!ok)
                        return;
                    loco.setFunction(functionId, newState);
                    if (loco === selectedLoco)
                        button.classList.toggle('active', newState);
                });
        }
    });
});

// Aiguillages : la page n'attend pas la fin de l'impulsion, l'ESP32
// coupe la bobine tout seul.
document.querySelectorAll('.turnout').forEach(turnout => {
    const address = turnout.getAttribute('data-address');
    turnout.querySelectorAll('.turnout-button').forEach(button => {
        button.addEventListener('click', () => {
            const straight = button.getAttribute('data-straight');
            send(`/setTurnout?address=${address}&straight=${straight}`, `Turnout ${address} ${straight === '1' ? 'straight' : 'round'}`)
                .then(ok => {
                    if (!ok)
                        return;
                    turnout.querySelectorAll('.turnout-button').forEach(b => b.classList.toggle('active', b === button));
                });
        });
    });
});

function updateSpeedValue(speed) {
    const speedValue = document.getElementById('speedValue');
    const percentage = (speed / 1000) * 100;
    speedValue.textContent = Math.round(percentage);
}
