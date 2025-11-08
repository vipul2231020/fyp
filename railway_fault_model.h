#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class DecisionTree {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        if (x[1] <= 0.5) {
                            if (x[0] <= 0.5) {
                                return 0;
                            }

                            else {
                                return 1;
                            }
                        }

                        else {
                            if (x[0] <= 0.5) {
                                return 2;
                            }

                            else {
                                return 3;
                            }
                        }
                    }

                protected:
                };
            }
        }
    }