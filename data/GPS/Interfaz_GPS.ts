//Variables dinamicas
let Velocidad_Actual = 0;
let Direccion_Actual = "adelante";
let Angulo_Rotacion = 0;
let Rpm_Actual = 0.0;

//Variables correlacionadas al HTML
const Slider_Velocidad     = document.getElementById("speedSlider");
const Display_Valor_Slider = document.getElementById("sliderValDisplay");
const Display_Porcentaje   = document.getElementById("percentDisplay");
const Display_Rpm          = document.getElementById("rpmDisplay");
const Display_Pulsos       = document.getElementById("pulsesDisplay");
const Display_Vueltas      = document.getElementById("turnsDisplay");
const Display_Delta        = document.getElementById("deltaDisplay");
const Display_Direccion    = document.getElementById("dirDisplay");
const Rotor_Visual         = document.getElementById("rotorVisual");
const Boton_Adelante       = document.getElementById("btnForward");
const Boton_Reversa        = document.getElementById("btnReverse");
const Log_Http             = document.getElementById("httpLog");
const Checkbox_Modo_Sim    = document.getElementById("simMode");
const Input_Ip_Esp         = document.getElementById("espIp");
const Badge_Conexion       = document.getElementById("connBadge");


Slider_Velocidad.addEventListener("input", (e) => {
  Actualizar_Velocidad(parseInt(e.target.value));
});

Checkbox_Modo_Sim.addEventListener("change", () => {
  Actualizar_Badge();
});

function Actualizar_Velocidad(Pwm_Val) {
  Velocidad_Actual = Pwm_Val;
  Display_Valor_Slider.textContent = Velocidad_Actual;
  const Porcentaje = Math.round((Velocidad_Actual / 255) * 100);
  Display_Porcentaje.textContent = `PWM: ${Velocidad_Actual} (${Porcentaje}%)`;
  Slider_Velocidad.value = Velocidad_Actual;

  Enviar_Comando_Motor();
}

function Establecer_Preset(Val) {
  Actualizar_Velocidad(Val);
}

function Establecer_Direccion(Dir) {
  Direccion_Actual = Dir;
  if (Dir === "adelante") {
    Boton_Adelante.classList.add("active");
    Boton_Reversa.classList.remove("active");
  } else {
    Boton_Reversa.classList.add("active");
    Boton_Adelante.classList.remove("active");
  }
  Enviar_Comando_Motor();
}

//Boton de paro de emergencia.
function Paro_De_Emergencia() {
  Actualizar_Velocidad(0);
}

let Tiempo_Ultimo_Envio = 0;
function Enviar_Comando_Motor() {
  const Tiempo_Actual = Date.now();
  if (Tiempo_Actual - Tiempo_Ultimo_Envio < 60 && Velocidad_Actual !== 0) return;
  Tiempo_Ultimo_Envio = Tiempo_Actual;

  const Es_Simulacion = Checkbox_Modo_Sim.checked;
  const Ip = Input_Ip_Esp.value.trim();
  const Url = `http://${Ip}/setMotor?speed=${Velocidad_Actual}&dir=${Direccion_Actual}`;

  Log_Http.textContent = `GET /setMotor?speed=${Velocidad_Actual}&dir=${Direccion_Actual}`;

  if (!Es_Simulacion) {
    fetch(Url, { method: "GET", mode: "no-cors" }).catch((err) => {
      Log_Http.textContent = `Error: ${err.message}`;
    });
  }
}

function Reiniciar_Odometria() {
  const Es_Simulacion = Checkbox_Modo_Sim.checked;
  const Ip = Input_Ip_Esp.value.trim();
  Log_Http.textContent = `GET /reset`;
  if (!Es_Simulacion) {
    fetch(`http://${Ip}/reset`, { method: "GET", mode: "no-cors" });
  } else {
    Display_Pulsos.textContent = "0";
    Display_Vueltas.textContent = "0.00";
  }
}

async function Obtener_Telemetria() {
  const Es_Simulacion = Checkbox_Modo_Sim.checked;
  if (Es_Simulacion) {
    Rpm_Actual =
      (Velocidad_Actual / 255) * 85.0 * (Direccion_Actual === "adelante" ? 1 : -1);
    Display_Rpm.textContent = Math.abs(Rpm_Actual).toFixed(1);
    Display_Direccion.textContent =
      Velocidad_Actual === 0
        ? "Paro"
        : Direccion_Actual === "adelante"
          ? "Adelante"
          : "Reversa";
    Display_Delta.textContent = Math.round((Rpm_Actual / 300) * 825);
    return;
  }

  const Ip = Input_Ip_Esp.value.trim();
  try {
    const Respuesta = await fetch(`http://${Ip}/telemetry`);
    if (Respuesta.ok) {
      const Datos = await Respuesta.json();
      Rpm_Actual = Datos.rpm;
      Display_Rpm.textContent = Math.abs(Datos.rpm).toFixed(1);
      Display_Pulsos.textContent = Datos.pulses;
      Display_Vueltas.textContent = Datos.turns.toFixed(2);
      Display_Delta.textContent = Datos.delta;
      Display_Direccion.textContent = Datos.dir;
      Badge_Conexion.textContent = "Conectado";
      Badge_Conexion.style.background = "#15803d";
    }
  } catch (e) {
    Badge_Conexion.textContent = "Sin conexión";
    Badge_Conexion.style.background = "#b91c1c";
  }
}
setInterval(Obtener_Telemetria, 200);

function Animar_Rotor() {
  if (Math.abs(Rpm_Actual) > 0.5) {
    const Paso = (Rpm_Actual / 60) * 360 * 0.016;
    Angulo_Rotacion += Paso;
    Rotor_Visual.style.transform = `rotate(${Angulo_Rotacion}deg)`;
  }
  requestAnimationFrame(Animar_Rotor);
}

Animar_Rotor();

function Actualizar_Badge() {
  if (Checkbox_Modo_Sim.checked) {
    Badge_Conexion.textContent = "Modo Simulación";
    Badge_Conexion.style.background = "#0369a1";
  } else {
    Badge_Conexion.textContent = "Buscando ESP32...";
    Badge_Conexion.style.background = "#eab308";
  }
}
Actualizar_Badge();