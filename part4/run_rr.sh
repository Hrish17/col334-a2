gnome-terminal -- bash -c "./server; exec bash"

tmux new-session -d -s mysession

# Split the tmux window into two panes
tmux split-window -h

# Run the server in the first pane
tmux send-keys -t mysession:0.0 "./server rr" C-m

# Run the client in the second pane
tmux send-keys -t mysession:0.1 "./client rr" C-m

# Attach to the tmux session to see both panes
tmux attach-session -t mysession

gnome-terminal -- bash -c "./client; exec bash"
