from flask import Flask, request, jsonify, render_template
import pickle
import numpy as np

app = Flask(__name__)

# Load trained model
with open("railway_fault_AI_with_confidence.pkl", "rb") as f:
    model = pickle.load(f)

status_map = {0: "Normal", 1: "Crack_Left", 2: "Crack_Right", 3: "Break"}

@app.route('/')
def home():
    return jsonify({"message": "API is live! Use POST /predict to get results."})

@app.route('/predict', methods=['POST'])
def predict():
    try:
        data = request.get_json(force=True)
        left = data.get("left_sensor")
        right = data.get("right_sensor")

        if left is None or right is None:
            return jsonify({"error": "Missing left_sensor or right_sensor"}), 400

        input_data = np.array([[left, right]])
        proba = model.predict_proba(input_data)[0]
        pred = np.argmax(proba)

        pred_class = status_map[pred]

        # --- Intelligent fault % calculation ---
        if pred_class == "Normal":
            fault_percentage = round((1 - proba[0]) * 100, 2)
        else:
            fault_percentage = round(proba[pred] * 100, 2)

        return jsonify({
            "prediction": pred_class,
            "probabilities": {
                "Normal": round(proba[0] * 100, 2),
                "Crack_Left": round(proba[1] * 100, 2),
                "Crack_Right": round(proba[2] * 100, 2),
                "Break": round(proba[3] * 100, 2)
            },
            "fault_percentage": fault_percentage
        })

    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=10000)
