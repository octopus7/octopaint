# Permanent Repository Instructions

## Request records

- Record every implementation or change request in `REQUESTS.md`.
- Keep the request-record filename in English.
- Add a new entry for each request; do not overwrite earlier entries.
- Write each entry title in this format:

  `# 요청 시작: YYYY-MM-DD HH:mm:ss KST | 작업 완료: YYYY-MM-DD HH:mm:ss KST (소요 시간: N시간 N분 N초)`

- Under the title, record these sections in order:

  `## 구현 내역`

  `## 결정 내용`

- Use the actual request start time and work completion time. Calculate and record the elapsed time from those timestamps.
- Complete the `REQUESTS.md` entry before reporting that the work is finished.

## Question and answer records

- Treat a simple question that does not request implementation or a repository change as a question-and-answer record.
- Record every simple question and its answer in `QUESTIONS.md`.
- Keep the question-record filename in English.
- Add a new entry for each question; do not overwrite earlier entries.
- Write the question heading in this format, with only the question time beside it:

  `## 질문 (HH:mm:ss KST)`

- Write the original question immediately below the heading, followed by:

  `### 답변`

- Record the answer below the answer heading.
- Do not add a completion time or elapsed time to a simple question entry.

## Parallel implementation work

- For every implementation or change request, identify work that can be completed independently.
- Delegate independent work to parallel agents when concurrency is available to reduce lead time.
- Apply the same rule to additional instructions received while work is already in progress.
- Give parallel agents non-overlapping file or subsystem ownership whenever possible.
- The primary agent remains responsible for integrating the results, resolving conflicts, running end-to-end verification, and completing the `REQUESTS.md` record.

## Commit discipline

- Commit each completed, logically distinct task independently.
- Do not mix unrelated changes in the same commit.
- Include the corresponding completed `REQUESTS.md` record in the commit for an implementation or change request.
- Do not commit partial work merely to separate files; a commit must represent a coherent completed outcome.
- When additional steering introduces a logically separate task, complete and commit it separately from the work already in progress.
- Push each successful commit to the configured remote immediately after creating it.
- If a push fails, report the failure and resolve it before treating the committed task as fully delivered.

## README localization

- Keep `README.md` as the default English README.
- Provide Korean and Japanese translations as `README_KO.md` and `README_JA.md`.
- Use an uppercase ISO-style two-letter language suffix for translated README filenames.
- At the top of each README, link to the other language versions of the same document; omit the link to the current document.
- Keep the three README files semantically equivalent whenever README content changes.
