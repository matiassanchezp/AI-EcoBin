from flask import Flask, request, jsonify
from flask_cors import CORS
from inference_sdk import InferenceHTTPClient
import csv
import os
import shutil
from datetime import datetime

app = Flask(__name__)
CORS(app)

CLIENT = InferenceHTTPClient(
    api_url="https://serverless.roboflow.com",
    api_key=os.environ.get("ROBOFLOW_API_KEY")
)

MODEL_ID = "organic-and-anorganic-waste/4"
LOG_PATH = "../data/logs/clasificaciones.csv"
ARCHIVE_DIR = "../data/logs/archivo"
CICLOS_PATH = "../data/logs/ciclos.csv"

estado_nivel = {"distancia_cm": None, "lleno": False, "actualizado": None}

def clasificar_categoria(nombre_clase):
    nombre_clase = nombre_clase.lower()
    if "anorgan" in nombre_clase or "inorgan" in nombre_clase:
        return "inorganico"
    elif "organ" in nombre_clase:
        return "organico"
    else:
        return "desconocido"

def guardar_log(categoria, clase_original, confianza):
    existe = os.path.isfile(LOG_PATH)
    with open(LOG_PATH, mode="a", newline="") as f:
        writer = csv.writer(f)
        if not existe:
            writer.writerow(["timestamp", "categoria", "clase_original", "confianza"])
        writer.writerow([datetime.now().isoformat(), categoria, clase_original, confianza])

@app.route("/classify", methods=["POST"])
def classify():
    try:
        imagen_bytes = request.get_data()
        temp_path = "temp_imagen.jpg"
        with open(temp_path, "wb") as f:
            f.write(imagen_bytes)
        resultado = CLIENT.infer(temp_path, model_id=MODEL_ID)
        predicciones = resultado.get("predictions", [])
        if not predicciones:
            return jsonify({"categoria": "desconocido", "confianza": 0.0})
        mejor = max(predicciones, key=lambda p: p["confidence"])
        nombre_clase = mejor["class"]
        confianza = round(mejor["confidence"], 3)
        categoria = clasificar_categoria(nombre_clase)
        guardar_log(categoria, nombre_clase, confianza)
        return jsonify({
            "categoria": categoria,
            "clase_detectada": nombre_clase,
            "confianza": confianza
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/history", methods=["GET"])
def history():
    if not os.path.isfile(LOG_PATH):
        return jsonify([])
    registros = []
    with open(LOG_PATH, mode="r", newline="") as f:
        reader = csv.DictReader(f)
        for fila in reader:
            registros.append(fila)
    registros.reverse()
    return jsonify(registros[:50])

@app.route("/fill_status", methods=["POST"])
def fill_status_post():
    try:
        datos = request.get_json()
        estado_nivel["distancia_cm"] = datos.get("distancia_cm")
        estado_nivel["lleno"] = datos.get("lleno", False)
        estado_nivel["actualizado"] = datetime.now().isoformat()
        return jsonify({"status": "ok"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/fill_status", methods=["GET"])
def fill_status_get():
    return jsonify(estado_nivel)

def registrar_ciclo():
    if not os.path.isfile(LOG_PATH):
        return

    with open(LOG_PATH, mode="r", newline="") as f:
        reader = list(csv.DictReader(f))

    if not reader:
        return

    inicio = reader[0]["timestamp"]
    fin = reader[-1]["timestamp"]
    total_organico = sum(1 for r in reader if r["categoria"] == "organico")
    total_inorganico = sum(1 for r in reader if r["categoria"] == "inorganico")

    existe = os.path.isfile(CICLOS_PATH)
    with open(CICLOS_PATH, mode="a", newline="") as f:
        writer = csv.writer(f)
        if not existe:
            writer.writerow(["inicio", "fin", "total_organico", "total_inorganico", "total_objetos"])
        writer.writerow([inicio, fin, total_organico, total_inorganico, total_organico + total_inorganico])

@app.route("/reset", methods=["POST"])
def reset():
    try:
        registrar_ciclo()

        if os.path.isfile(LOG_PATH):
            os.makedirs(ARCHIVE_DIR, exist_ok=True)
            marca_tiempo = datetime.now().strftime("%Y%m%d_%H%M%S")
            destino = os.path.join(ARCHIVE_DIR, f"clasificaciones_{marca_tiempo}.csv")
            shutil.move(LOG_PATH, destino)

        return jsonify({"status": "ok", "mensaje": "Caneca vaciada, ciclo registrado y contador reiniciado"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/ciclos", methods=["GET"])
def ciclos():
    if not os.path.isfile(CICLOS_PATH):
        return jsonify([])
    registros = []
    with open(CICLOS_PATH, mode="r", newline="") as f:
        reader = csv.DictReader(f)
        for fila in reader:
            registros.append(fila)
    registros.reverse()
    return jsonify(registros)

@app.route("/", methods=["GET"])
def home():
    return "Servidor de clasificacion de basura activo"

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
