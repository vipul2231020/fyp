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
        data = request.get_json(force=True)  # ✅ force=True fixes bad request
        left = data.get("left_sensor")
        right = data.get("right_sensor")

        # input validation
        if left is None or right is None:
            return jsonify({"error": "Missing left_sensor or right_sensor"}), 400

        input_data = np.array([[left, right]])
        pred = model.predict(input_data)[0]
        proba = model.predict_proba(input_data)[0]

        pred_class = status_map[pred]
        fault_percentage = round(proba[pred] * 100, 2)

        return jsonify({
            "prediction": pred_class,
            "confidence": round(proba[pred], 2),
            "fault_percentage": fault_percentage
        })

    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=10000)
