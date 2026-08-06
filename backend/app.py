from flask import Flask, request, jsonify
from inference_sdk import InferenceHTTPClient
import csv
import os
from datetime import datetime

app = Flask(__name__)

CLIENT = InferenceHTTPClient(
    api_url="https://serverless.roboflow.com",
    api_key=os.environ.get("ROBOFLOW_API_KEY")
)

MODEL_ID = "organic-and-anorganic-waste/4"
LOG_PATH = "../data/logs/clasificaciones.csv"

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

@app.route("/", methods=["GET"])
def home():
    return "Servidor de clasificacion de basura activo"

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
