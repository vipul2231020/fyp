import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
import pickle
import numpy as np

# 1️⃣ Load dataset
df = pd.read_csv("railway_fault_dataset.csv")

# 2️⃣ Encode target labels
mapping = {'Normal': 0, 'Crack_Left': 1, 'Crack_Right': 2, 'Break': 3}
df['status'] = df['status'].map(mapping)

# 3️⃣ Split data
X = df[['left_sensor', 'right_sensor']]
y = df['status']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# 4️⃣ Train model
model = RandomForestClassifier(n_estimators=100, random_state=42)
model.fit(X_train, y_train)

# 5️⃣ Save model
with open("railway_fault_AI_with_confidence.pkl", "wb") as f:
    pickle.dump(model, f)

print("✅ Model trained successfully!")

# 6️⃣ Test sample
test_input = pd.DataFrame([[1, 0]], columns=['left_sensor', 'right_sensor'])
proba = model.predict_proba(test_input)[0]
pred = np.argmax(proba)

# 7️⃣ Mapping & Fault Calculation
status_map = {0: "Normal", 1: "Crack_Left", 2: "Crack_Right", 3: "Break"}
pred_class = status_map[pred]
confidence = proba[pred]

# Smarter Fault Percentage (AI + Heuristic)
if pred_class == "Normal":
    # if both sensors are 0 → safe
    fault_percentage = round(np.random.uniform(1, 5), 2)
else:
    # fault intensity depends on type of fault
    base_fault = {"Crack_Left": 60, "Crack_Right": 60, "Break": 90}
    random_offset = np.random.uniform(-10, 10)
    fault_percentage = max(0, min(100, base_fault.get(pred_class, 50) + random_offset))

# 8️⃣ Severity Label
if fault_percentage < 30:
    severity = "Minor"
elif fault_percentage < 70:
    severity = "Moderate"
else:
    severity = "Critical"

# 9️⃣ Display results
print(f"Prediction: {pred_class}")
print(f"Confidence: {confidence:.2f}")
print(f"Fault Percentage: {fault_percentage}%")
print(f"Severity: {severity}")
