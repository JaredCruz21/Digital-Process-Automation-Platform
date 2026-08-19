# Modeling Software Evaluation

## Purpose

This document compares software that could be used to model the brewing automation process. The required toolchain needs to support three related but different jobs:

1. Model the thermal, liquid-volume, and time-dependent behavior of the physical equipment.
2. Estimate parameters and make predictions from real controller logs.
3. Display process state, history, alarms, and model results in an industrial-style interface.

No single tool is the best choice for all three. A layered toolchain provides more useful learning experience and lets each part remain replaceable.

## Recommendation

Start with **Python as the core modeling environment**. Add **OpenModelica** after the first thermal model has been validated. Add **Ignition Maker Edition** later when the project is ready for a SCADA and historian layer.

```text
Teensy 4.1 controller
        |
        | CSV now; serial or MQTT later
        v
Python data and modeling pipeline
        |
        +--> measured vs. predicted results
        +--> browser digital process view
        +--> OpenModelica physical simulation
        |
        v
Ignition Maker Edition SCADA layer (later phase)
```

This approach fits the existing C++ firmware and CSV logs, keeps the early work free and version-controlled, and adds industrial software only when it solves a real need.

## Software Comparison

| Software | Best use in this project | Fit | Main tradeoff | Decision |
| --- | --- | --- | --- | --- |
| Python, SciPy, pandas, scikit-learn | Data cleaning, differential-equation models, parameter fitting, prediction, validation | Excellent | More code and less graphical modeling | Start here |
| OpenModelica and OMEdit | Equation-based thermal and fluid process simulation | Strong | Modelica has a learning curve | Add after the Python baseline |
| MATLAB, Simulink, Simscape, Stateflow | Integrated physical modeling, control simulation, state machines, and system identification | Excellent if licensed | Product and toolbox licensing | Best all-in-one alternative |
| Ignition Maker Edition | SCADA screens, tags, history, alarms, SQL, OPC UA, and MQTT integration | Strong for automation learning | Not a physics or machine-learning engine; personal non-commercial license | Add as the industrial interface |
| Node-RED | USB serial, MQTT, transformation, and edge data routing | Useful supporting tool | Flows can become hard to maintain if used as the model itself | Use only as an integration bridge |
| InfluxDB and Grafana | Time-series storage and dashboards | Useful alternative | Adds two more services and overlaps with Ignition | Consider if a lightweight telemetry stack is preferred |
| Factory I/O | 3D PLC and discrete factory automation simulation | Limited for this process | Oriented toward conveyors, parts, tanks, and PLC I/O rather than detailed batch thermal behavior | Save for a separate PLC project |

## Why Python Should Come First

Python connects directly to the CSV files already produced by the firmware. It can be used to clean logs, align event records, fit physical parameters, evaluate errors, and provide predictions to the existing browser application.

The first model can be a simple energy balance:

```text
m * Cp * dT/dt = efficiency * heater_power * heater_state
                 - heat_loss_coefficient * (T - ambient_temperature)
```

For cooling, the heater term is removed and the heat-transfer term is adjusted to represent the cooling system. The unknown efficiency and heat-loss parameters can be estimated from complete process runs.

[SciPy provides numerical ordinary differential equation solvers](https://docs.scipy.org/doc/scipy/reference/generated/scipy.integrate.solve_ivp.html), and scikit-learn provides regression and model-evaluation tools. Time-dependent data must be validated in chronological order; [scikit-learn specifically recommends time-series-aware splitting](https://scikit-learn.org/stable/modules/cross_validation.html#cross-validation-of-time-series-data) instead of random cross-validation for this type of data.

This first-principles model should become the baseline. Machine learning can then model the remaining prediction error instead of trying to learn the entire physical process from a small dataset.

## Why Add OpenModelica

[OpenModelica](https://openmodelica.org/) is a free, open-source environment intended for industrial, academic, and teaching use. Its graphical editor and equation-based modeling are well suited to thermal mass, heat transfer, pumps, vessels, and connected physical components.

It also supports [Python integration through OMPython](https://openmodelica.org/doc/OpenModelicaUsersGuide/latest/ompython.html) and model exchange through the Functional Mock-up Interface. That makes it possible to run a physical simulation from the same Python workflow used to process controller logs.

OpenModelica is most useful after a smaller Python model works. At that point, each added component can be justified by a real effect observed in the data, such as heater delay, environmental heat loss, changing liquid mass, boil-off, or cooling-water temperature.

## MATLAB And Simulink Alternative

If a school, employer, or personal license is available, the MathWorks stack is the strongest all-in-one option:

- [Simscape](https://www.mathworks.com/products/simscape.html) models connected physical systems across thermal, fluid, electrical, and other domains.
- [Stateflow](https://www.mathworks.com/products/stateflow.html) models and simulates state-machine and supervisory logic.
- [System Identification Toolbox](https://www.mathworks.com/products/sysid.html) estimates dynamic models from measured input and output data.

This maps closely to the project's physical process, firmware states, and logged data. Its main disadvantage is dependence on several licensed products. The underlying modeling lessons transfer between Simulink and the recommended open-source stack, so beginning in Python does not close off this path.

## Why Ignition Is A Later Phase

[Ignition Maker Edition](https://inductiveautomation.com/ignition/maker-edition) is free for personal, non-commercial projects and includes industrial concepts such as tags, SQL, OPC UA, MQTT, historian features, and browser-based Perspective screens. This makes it a strong platform for learning SCADA architecture with the finished controller.

Ignition should consume process data and model outputs; it should not be responsible for the physical equations or model training. Adding it after the data schema and model API are stable keeps the architecture understandable.

Maker Edition is limited to personal, non-commercial use. Its official documentation also notes limits and conditions that should be reviewed before using it for a public or commercial demonstration.

## Supporting Tools

### Node-RED

[Node-RED](https://nodered.org/) is designed to collect, transform, and visualize event-driven data and can run on an edge computer such as a Raspberry Pi. It is a practical option for reading USB serial data from the Teensy and publishing structured messages to a database, Python service, or MQTT broker.

It is optional while the system uses SD-card files. Add it when live telemetry becomes a milestone.

### InfluxDB And Grafana

[InfluxDB 3 Core](https://docs.influxdata.com/influxdb3/core/get-started/) is designed for real-time monitoring data, while [Grafana](https://grafana.com/docs/grafana/latest/visualizations/panels-visualizations/visualizations/time-series/) provides time-series dashboards. They form a capable open telemetry stack, but they overlap with the learning goals served by Ignition.

Use this pair instead of Ignition if the priority becomes software observability and open dashboards rather than SCADA architecture.

### Factory I/O

[Factory I/O](https://docs.factoryio.com/) is a 3D factory simulator primarily designed for PLC training. It includes tank filling and level-control scenes and supports PLC, Modbus, OPC, and some microcontroller workflows.

It is useful for learning discrete manufacturing and PLC integration, but it is not the best core model for this project. The most important brewing behavior is temperature and batch-process dynamics, while Factory I/O is strongest at visual equipment interaction and discrete I/O.

## Proposed Learning Sequence

1. Standardize the existing CSV sample and event formats.
2. Load and graph complete runs in Python.
3. Implement the heating and cooling energy-balance model.
4. Fit physical parameters from one group of runs.
5. Validate against separate, later runs using MAE and stage ETA error.
6. Feed measured and predicted traces into the browser visualization.
7. Rebuild the physical model in OpenModelica and compare outputs.
8. Add residual anomaly detection only after the baseline is reliable.
9. Stream live data through serial or MQTT when offline replay works.
10. Add Ignition tags, history, alarms, and Perspective screens.

## Final Choice

The recommended core is **Python plus OpenModelica**. Python provides the shortest path from the existing logs to a tested prediction, while OpenModelica adds formal physical-system modeling. **Ignition Maker Edition** is the best later addition for broadening the project into industrial SCADA and automation architecture.

MATLAB, Simulink, Simscape, and Stateflow should replace the Python and OpenModelica combination only when a suitable license is available and learning that ecosystem is a specific objective.
