var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __generator = (this && this.__generator) || function (thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g = Object.create((typeof Iterator === "function" ? Iterator : Object).prototype);
    return g.next = verb(0), g["throw"] = verb(1), g["return"] = verb(2), typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (g && (g = 0, op[0] && (_ = 0)), _) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
};
//Variables dinamicas
var Velocidad_Actual = 0;
var Direccion_Actual = "adelante";
var Angulo_Rotacion = 0;
var Rpm_Actual = 0.0;
//Variables correlacionadas al HTML
var Slider_Velocidad = document.getElementById("speedSlider");
var Display_Valor_Slider = document.getElementById("sliderValDisplay");
var Display_Porcentaje = document.getElementById("percentDisplay");
var Display_Rpm = document.getElementById("rpmDisplay");
var Display_Pulsos = document.getElementById("pulsesDisplay");
var Display_Vueltas = document.getElementById("turnsDisplay");
var Display_Delta = document.getElementById("deltaDisplay");
var Display_Direccion = document.getElementById("dirDisplay");
var Rotor_Visual = document.getElementById("rotorVisual");
var Boton_Adelante = document.getElementById("btnForward");
var Boton_Reversa = document.getElementById("btnReverse");
var Log_Http = document.getElementById("httpLog");
var Checkbox_Modo_Sim = document.getElementById("simMode");
var Input_Ip_Esp = document.getElementById("espIp");
var Badge_Conexion = document.getElementById("connBadge");
Slider_Velocidad.addEventListener("input", function (e) {
    Actualizar_Velocidad(parseInt(e.target.value));
});
Checkbox_Modo_Sim.addEventListener("change", function () {
    Actualizar_Badge();
});
function Actualizar_Velocidad(Pwm_Val) {
    Velocidad_Actual = Pwm_Val;
    Display_Valor_Slider.textContent = Velocidad_Actual;
    var Porcentaje = Math.round((Velocidad_Actual / 255) * 100);
    Display_Porcentaje.textContent = "PWM: ".concat(Velocidad_Actual, " (").concat(Porcentaje, "%)");
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
    }
    else {
        Boton_Reversa.classList.add("active");
        Boton_Adelante.classList.remove("active");
    }
    Enviar_Comando_Motor();
}
//Boton de paro de emergencia.
function Paro_De_Emergencia() {
    Actualizar_Velocidad(0);
}
var Tiempo_Ultimo_Envio = 0;
function Enviar_Comando_Motor() {
    var Tiempo_Actual = Date.now();
    if (Tiempo_Actual - Tiempo_Ultimo_Envio < 60 && Velocidad_Actual !== 0)
        return;
    Tiempo_Ultimo_Envio = Tiempo_Actual;
    var Es_Simulacion = Checkbox_Modo_Sim.checked;
    var Ip = Input_Ip_Esp.value.trim();
    var Url = "http://".concat(Ip, "/setMotor?speed=").concat(Velocidad_Actual, "&dir=").concat(Direccion_Actual);
    Log_Http.textContent = "GET /setMotor?speed=".concat(Velocidad_Actual, "&dir=").concat(Direccion_Actual);
    if (!Es_Simulacion) {
        fetch(Url, { method: "GET", mode: "no-cors" }).catch(function (err) {
            Log_Http.textContent = "Error: ".concat(err.message);
        });
    }
}
function Reiniciar_Odometria() {
    var Es_Simulacion = Checkbox_Modo_Sim.checked;
    var Ip = Input_Ip_Esp.value.trim();
    Log_Http.textContent = "GET /reset";
    if (!Es_Simulacion) {
        fetch("http://".concat(Ip, "/reset"), { method: "GET", mode: "no-cors" });
    }
    else {
        Display_Pulsos.textContent = "0";
        Display_Vueltas.textContent = "0.00";
    }
}
function Obtener_Telemetria() {
    return __awaiter(this, void 0, void 0, function () {
        var Es_Simulacion, Ip, Respuesta, Datos, e_1;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    Es_Simulacion = Checkbox_Modo_Sim.checked;
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
                        return [2 /*return*/];
                    }
                    Ip = Input_Ip_Esp.value.trim();
                    _a.label = 1;
                case 1:
                    _a.trys.push([1, 5, , 6]);
                    return [4 /*yield*/, fetch("http://".concat(Ip, "/telemetry"))];
                case 2:
                    Respuesta = _a.sent();
                    if (!Respuesta.ok) return [3 /*break*/, 4];
                    return [4 /*yield*/, Respuesta.json()];
                case 3:
                    Datos = _a.sent();
                    Rpm_Actual = Datos.rpm;
                    Display_Rpm.textContent = Math.abs(Datos.rpm).toFixed(1);
                    Display_Pulsos.textContent = Datos.pulses;
                    Display_Vueltas.textContent = Datos.turns.toFixed(2);
                    Display_Delta.textContent = Datos.delta;
                    Display_Direccion.textContent = Datos.dir;
                    Badge_Conexion.textContent = "Conectado";
                    Badge_Conexion.style.background = "#15803d";
                    _a.label = 4;
                case 4: return [3 /*break*/, 6];
                case 5:
                    e_1 = _a.sent();
                    Badge_Conexion.textContent = "Sin conexión";
                    Badge_Conexion.style.background = "#b91c1c";
                    return [3 /*break*/, 6];
                case 6: return [2 /*return*/];
            }
        });
    });
}
setInterval(Obtener_Telemetria, 200);
function Animar_Rotor() {
    if (Math.abs(Rpm_Actual) > 0.5) {
        var Paso = (Rpm_Actual / 60) * 360 * 0.016;
        Angulo_Rotacion += Paso;
        Rotor_Visual.style.transform = "rotate(".concat(Angulo_Rotacion, "deg)");
    }
    requestAnimationFrame(Animar_Rotor);
}
Animar_Rotor();
function Actualizar_Badge() {
    if (Checkbox_Modo_Sim.checked) {
        Badge_Conexion.textContent = "Modo Simulación";
        Badge_Conexion.style.background = "#0369a1";
    }
    else {
        Badge_Conexion.textContent = "Buscando ESP32...";
        Badge_Conexion.style.background = "#eab308";
    }
}
Actualizar_Badge();
