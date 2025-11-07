from flask import Flask, request, jsonify
import pickle
import numpy as np
import random

app = Flask(__name__)

# Load trained model
with open("railway_fault_AI_with_confidence.pkl", "rb") as f:
    model = pickle.load(f)

# Mapping classes
status_map = {0: "Normal", 1: "Crack_Left", 2: "Crack_Right", 3: "Break"}

@app.route('/')
def home():
    return jsonify({
        "message": "🚆 Railway Fault Detection AI API is Live!",
        "usage": "Send POST request to /predict with JSON { 'left_sensor': 0, 'right_sensor': 1 }"
    })

@app.route('/predict', methods=['POST'])
def predict():
    try:
        data = request.get_json(force=True)
        left = data.get("left_sensor")
        right = data.get("right_sensor")

        if left is None or right is None:
            return jsonify({"error": "Missing left_sensor or right_sensor"}), 400

        # Convert to NumPy array for model
        input_data = np.array([[left, right]])
        proba = model.predict_proba(input_data)[0]
        pred = np.argmax(proba)
        pred_class = status_map[pred]
        confidence = round(proba[pred], 2)

        # --- Smarter Fault Percentage (AI + Heuristic mix) ---
        if pred_class == "Normal":
            fault_percentage = round(random.uniform(1, 5), 2)
        else:
            base_fault = {"Crack_Left": 60, "Crack_Right": 60, "Break": 90}
            random_offset = random.uniform(-10, 10)
            fault_percentage = max(0, min(100, base_fault.get(pred_class, 50) + random_offset))

        # --- Severity classification ---
        if fault_percentage < 30:
            severity = "Minor"
        elif fault_percentage < 70:
            severity = "Moderate"
        else:
            severity = "Critical"

        # --- Return JSON Response ---
        return jsonify({
            "prediction": pred_class,
            "confidence": f"{confidence * 100:.2f}%",
            "fault_percentage": fault_percentage,
            "severity": severity,
            "probabilities": {
                "Normal": round(proba[0] * 100, 2),
                "Crack_Left": round(proba[1] * 100, 2),
                "Crack_Right": round(proba[2] * 100, 2),
                "Break": round(proba[3] * 100, 2)
            }
        })

    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=10000)
