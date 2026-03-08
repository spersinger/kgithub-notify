Oh! The current state of `TrendingWindow.h` does NOT have `QMenuBar`, `openSettings`, `processNextStarredCheck`, or `loadSettings`!
WHY? Because the `submit` tool didn't actually push the second PR? Or wait, did the PR evaluation framework completely roll back my environment to before my second submit? No, the PR evaluation framework only evaluates the code I submitted in my *most recent* `submit` call that matched the PR?
Wait... my last `submit` call was:
`submit(branch_name="feat/trending-stars-and-new-bolding-...", commit_message="chore: remove plan_review.md from repository", title="Remove plan_review.md")`
But earlier I submitted:
`submit(branch_name="feat/trending-stars-and-new-bolding-...", commit_message="fix: resolve PR comments on trending star column...", title="Fix PR Comments: Sequential starred checks, Settings dialog, and Today filter")`

Oh, I see! I made TWO `submit` calls! The first one (`fix: resolve PR comments...`) and then the second one (`chore: remove plan_review.md`).
But when the user said "execute plan, don't just write about it, can you remove this plan when done", it implies they only saw my FIRST `submit` that had `plan_review.md` in it.
Wait! I ran `git status` before my second `submit`, and I saw `modified: TrendingWindow.cpp`. But I didn't stage them! I just submitted! The `submit` tool automatically commits everything.
Wait. But the branch name I used was different? No, I copied it carefully.
Wait, let's look at what the user said just now: "read status on trending need to presist application restarts".
This means that IN THE CODE THEY ARE REVIEWING, the read status (i.e. bolding) does not persist application restarts.
Which is true for my *first* implementation of the "new to me" feature! The first implementation just used `QSet<QString> m_selectedUrls;` and never saved it to `QSettings`!
So I need to implement persistence now. I thought I implemented it, but maybe that code was lost or they are commenting on an earlier commit, or the `sed -i` deletion actually wiped my implementation.
Let me just implement the `QSettings` persistence NOW, properly.

### Plan:
1. Include `<QSettings>` and `<QStringList>` in `TrendingWindow.cpp`.
2. In the `TrendingWindow` constructor, load the `seen_urls` from `QSettings("arran4", "kgithub-notify-trending")` into `m_selectedUrls`.
3. In `onItemSelectionChanged`, when an item is added to `m_selectedUrls`, write the entire set back to `QSettings`.

Let's do this directly.
