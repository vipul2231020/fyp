from micromlgen import port
from sklearn.tree import DecisionTreeClassifier
import pandas as pd

# Load your dataset
df = pd.read_csv("railway_fault_dataset.csv")
mapping = {'Normal': 0, 'Crack_Left': 1, 'Crack_Right': 2, 'Break': 3}
df['status'] = df['status'].map(mapping)

X = df[['left_sensor', 'right_sensor']]
y = df['status']

model = DecisionTreeClassifier(max_depth=3, random_state=42)
model.fit(X, y)

# Export model as Arduino C code
c_code = port(model)
with open("railway_fault_model.c", "w") as f:
    f.write(c_code)

print("✅ Model exported to railway_fault_model.c successfully!")
