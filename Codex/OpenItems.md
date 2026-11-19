# Open-item workflow

Use the [open-item index](../Docs/OpenItems/Index.md) for a pending decision, known defect, risk, or missing check that can affect future work. The index holds a short title, topic, and status. Each card holds the details for one outcome.

## Starting a task

Check the rows for the affected system, or open the card named in the task. Open a card only when its decision, risk, or check could change this work. Follow evidence and design links to the relevant passage. If an open decision blocks the task, ask the user and continue independent work.

## Recording a finding

Search the index by topic for an existing card. Update it when it already tracks the same outcome. Otherwise add `Docs/OpenItems/GS-<number>-<Title>.md` with the next unused number and one row in the index. Keep the identifier stable. Include the topic, current status, concrete question or observation, closure condition, and a link to the evidence or owning document. Link the card from the specific section likely to need it.

Record the status in the index and card together.

| Status in the register | Use when |
| --- | --- |
| Открыто | A decision or fix is pending. |
| К проверке | A hypothesis, observation, or missing check needs evidence. |
| Закрыто | The decision or verification was recorded and meets the closure condition. |

## Finishing a task

Update affected cards with decisions or checks that actually occurred. Put lasting rules in the owning design or technical document. For a closed card, record its result and link to the decision or verification, then remove its row from the active index. Keep the card at its existing path for references. Use the next number based on both active and closed filenames.

Use [work reports](WorkReports.md) for task history. Cards link to durable evidence or briefly record the relevant result when the detailed evidence is local.
